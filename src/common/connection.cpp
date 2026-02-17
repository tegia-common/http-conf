#include <tegia/core/string.h>
#include <tegia/core/cast.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "connection.h"


////////////////////////////////////////////////////////////////////////////////////////////
#undef _LOG_LEVEL_
#define _LOG_LEVEL_ _LOG_DEBUG_
#include <tegia/context/log.h>
////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////


connection_t::connection_t()
{
	this->start_time = std::chrono::high_resolution_clock::now();
	this->req = new FCGX_Request;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////


connection_t::~connection_t() 
{
	delete this->req;

	this->end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();

	std::cout 	<< _YELLOW_ << "[" << this->status << "] REQUEST " << _BASE_TEXT_ 
				<< this->url << " (time: " << duration << ")"  << std::endl;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////


inline std::string get_param(const char* param_name, FCGX_Request *req)
{
	if(FCGX_GetParam(param_name, req->envp) != nullptr)
	{
		return FCGX_GetParam(param_name, req->envp);
	}
	else
	{
		return "";
	}
};


static inline std::string trim(const std::string &value)
{
	auto start = value.find_first_not_of(" \t\r\n");
	if(start == std::string::npos)
	{
		return "";
	}
	auto end = value.find_last_not_of(" \t\r\n");
	return value.substr(start, end - start + 1);
}


static std::string extract_disposition_param(const std::string &header, const std::string &key)
{
	const std::string marker = key + "=\"";
	auto start = header.find(marker);
	if(start == std::string::npos)
	{
		return "";
	}
	start += marker.size();
	auto end = header.find('"', start);
	if(end == std::string::npos)
	{
		return "";
	}
	return header.substr(start, end - start);
}


static std::string extract_boundary(const std::string &content_type)
{
	const std::string marker = "boundary=";
	auto pos = content_type.find(marker);
	if(pos == std::string::npos)
	{
		return "";
	}
	auto boundary = trim(content_type.substr(pos + marker.size()));
	if(boundary.empty())
	{
		return "";
	}

	if(boundary.front() == '"' && boundary.back() == '"' && boundary.size() > 1)
	{
		boundary = boundary.substr(1, boundary.size() - 2);
	}

	return boundary;
}


static bool write_binary_file(const std::string &content, std::string &path)
{
	std::array<char, 64> tmp{};
	std::strcpy(tmp.data(), "/tmp/http-conf-upload-XXXXXX");

	int fd = mkstemp(tmp.data());
	if(fd == -1)
	{
		return false;
	}

	ssize_t written = 0;
	while(written < static_cast<ssize_t>(content.size()))
	{
		auto res = write(fd, content.data() + written, content.size() - written);
		if(res <= 0)
		{
			close(fd);
			unlink(tmp.data());
			return false;
		}
		written += res;
	}

	close(fd);
	path = tmp.data();
	return true;
}


static void ensure_upload_data_object(nlohmann::json &post)
{
	if(!post.contains("data") || !post["data"].is_object())
	{
		post["data"] = nlohmann::json::object();
	}
}


bool connection_t::init(const std::string &name)
{
	//
	// REM: Записывает в лог полный список всех заголовков
	//

	std::string query;
	
	#if _LOG_LEVEL_ == _LOG_DEBUG_
	{
		int i = 0;

		query = query + "CONNECTION " + name + "\n";

		while(this->req->envp[i] != nullptr)
		{
			char * str = this->req->envp[i];
			char * position = strchr(str, '=') + sizeof(str[0]);
			int p = strlen(str) - strlen(position) - 1;
			i++;

			// std::cout << "param = " << std::string(str,p) << std::endl;
			// std::cout << "value = " << std::string(position) << std::endl;
			// std::cout << " " << std::endl;

			query = query + std::string(str,p) + " = '" + std::string(position) + "'\n";
		}

		LDEBUG(query)
	}
	#endif

	//
	// CGI Query Information
	//

	this->request_method = get_param("REQUEST_METHOD", this->req);
	this->script_name = get_param("SCRIPT_NAME", this->req);
	this->content_type = get_param("CONTENT_TYPE", this->req);
	this->content_length = get_param("CONTENT_LENGTH", this->req);
	this->redirect_status = get_param("REDIRECT_STATUS", this->req);

	//
	// Information on the server handling the HTTP/CGI request
	//
	
	this->server_software = get_param("SERVER_SOFTWARE", this->req);
	this->server_name = get_param("SERVER_NAME", this->req);
	this->gateway_interface = get_param("GATEWAY_INTERFACE", this->req);
	this->server_protocol = get_param("SERVER_PROTOCOL", this->req);
	this->server_port = get_param("SERVER_PORT", this->req);
	
	//
	// Remote User Information. Information about the user making the CGI request
	//

	this->remote_addr = get_param("REMOTE_ADDR", this->req);
	this->http_accept = get_param("HTTP_ACCEPT", this->req);
	this->http_user_agent = get_param("HTTP_USER_AGENT", this->req);
	this->http_referer = get_param("HTTP_REFERER", this->req);
	this->http_origin = get_param("HTTP_ORIGIN", this->req);
	this->http_authorization = get_param("HTTP_AUTHORIZATION", this->req);

	//
	// Get a COOKIE
	//

	if(FCGX_GetParam("HTTP_COOKIE", this->req->envp) != nullptr)
	{
		std::string cookie = std::string(FCGX_GetParam("HTTP_COOKIE", this->req->envp));

		#if _LOG_LEVEL_ == _LOG_DEBUG_
		{
			query = query + "Cookies: " + cookie + "\n";
			LDEBUG(query);
		}
		#endif

		auto cookie_arr = tegia::string::explode(cookie,"; ");
		for(auto it = cookie_arr.begin(); it != cookie_arr.end(); it++)
		{
			auto tmp = tegia::string::explode((*it),"=");
			this->cookie.insert({tmp[0],tmp[1]});
		}
	}


	//
	// Get a POST data
	//

	int content_length = 0;
	if(this->content_length != "")
	{
		content_length = core::cast<int>(this->content_length);
	}
	
	if(content_length > 0)
	{
		char *buff = new char[content_length+1]{};
		FCGX_GetStr(buff,content_length,this->req->in);
		std::string post = std::string(buff, content_length);
		delete[] buff;

		#if _LOG_LEVEL_ == _LOG_DEBUG_
		{
			query = query + "POST: " + post + "\n";
			LDEBUG(query);
		}
		#endif

		//
		// Проверяем, что POST-данные содержат валидный JSON
		//

		if(this->content_type.find("multipart/form-data") != std::string::npos)
		{
			const auto boundary = extract_boundary(this->content_type);
			if(boundary.empty())
			{
				LDEBUG("multipart/form-data without boundary");
			}
			else
			{
				const std::string delimiter = "--" + boundary;
				size_t search_pos = 0;

				while(true)
				{
					auto part_begin = post.find(delimiter, search_pos);
					if(part_begin == std::string::npos)
					{
						break;
					}

					part_begin += delimiter.size();
					if(part_begin + 1 < post.size() && post.compare(part_begin, 2, "--") == 0)
					{
						break;
					}

					if(post.compare(part_begin, 2, "\r\n") == 0)
					{
						part_begin += 2;
					}

					auto headers_end = post.find("\r\n\r\n", part_begin);
					if(headers_end == std::string::npos)
					{
						break;
					}

					auto next_delimiter = post.find("\r\n" + delimiter, headers_end + 4);
					if(next_delimiter == std::string::npos)
					{
						break;
					}

					std::string headers = post.substr(part_begin, headers_end - part_begin);
					std::string data = post.substr(headers_end + 4, next_delimiter - (headers_end + 4));

					std::string name;
					std::string filename;
					std::string file_content_type;

					auto header_lines = tegia::string::explode(headers, "\r\n");
					for(auto &line : header_lines)
					{
						if(line.find("Content-Disposition:") == 0)
						{
							name = extract_disposition_param(line, "name");
							filename = extract_disposition_param(line, "filename");
						}

						if(line.find("Content-Type:") == 0)
						{
							auto p = line.find(':');
							if(p != std::string::npos)
							{
								file_content_type = trim(line.substr(p + 1));
							}
						}
					}

					if(!filename.empty())
					{
						std::string stored_path;
						if(write_binary_file(data, stored_path))
						{
							nlohmann::json metadata;
							metadata["field"] = name;
							metadata["filename"] = filename;
							metadata["content_type"] = file_content_type;
							metadata["size"] = data.size();
							metadata["path"] = stored_path;

							this->uploaded_files.push_back(metadata);
						}
					}
					else if(!name.empty())
					{
						if(name == "data")
						{
							try
							{
								this->post["data"] = nlohmann::json::parse(data);
							}
							catch(const nlohmann::json::parse_error&)
							{
								this->post["data"] = data;
							}
						}
						else
						{
							this->post[name] = data;
						}
					}

					search_pos = next_delimiter + 2;
				}

				if(!this->uploaded_files.empty())
				{
					ensure_upload_data_object(this->post);
					this->post["data"]["_uploaded_files"] = this->uploaded_files;
				}
			}
		}
		else
		{
			try
			{
				nlohmann::json _post = nlohmann::json::parse(post);
				if(_post.is_array() == true)
				{
					this->post["post"] = _post;
				}
				else
				{
					this->post = _post;
				}
			}

		//
		// Обрабатываем классические POST-данные
		//

			catch(nlohmann::json::parse_error& e)
			{
				LDEBUG("POST RAW data: " + post);
				std::cout << "POST RAW data: " << post << std::endl;

			/*
			auto params = core::explode(post,"&",true);
			for(auto it = params.begin(); it != params.end(); it++)
			{
				auto param = core::explode( (*it),"=",true);
				if(param.size() == 1)
				{
					message->http->request.post[param[0]] = ""; 
				}
				else
				{
					message->http->request.post[param[0]] = tegia::http::unescape(param[1]); 
				}
			}
			*/
			}	
		}
	}



	return true;
};


