#ifndef H_HTTP_FASTCGI_CONNECTION
#define H_HTTP_FASTCGI_CONNECTION

	#include <tegia/core/const.h>
	#include <tegia/core/json.h>

//	VENDORS: FastCGI
	#include <unistd.h>
	#include <fcgi_stdio.h>
	#include <cgicc/CgiDefs.h>
	#include <cgicc/Cgicc.h>
	#include <cgicc/HTTPHTMLHeader.h>
	#include <cgicc/HTMLClasses.h>
	using namespace cgicc;
	
	#include <chrono>


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// connection_t CLASS                                                                     //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////



class connection_t
{
	private:

		std::chrono::high_resolution_clock::time_point start_time;
		std::chrono::high_resolution_clock::time_point end_time;

		int _application_json(const long long int content_length);
		int _multipart_form_data(const long long int content_length);

	public:
		connection_t();

		~connection_t();

		int init(const std::string &name);

		//
		// REQUEST PARAM
		//

		std::string request_method{};
		std::string script_name{};
		std::string content_type{};
		std::string content_length{};
		std::string redirect_status{};

		std::string server_software{};
		std::string server_name{};
		std::string gateway_interface{};
		std::string server_protocol{};
		std::string server_port{};

		std::string remote_addr{};
		std::string http_accept{};
		std::string http_user_agent{};
		std::string http_referer{};
		std::string http_origin{};
		std::string http_authorization{};

		nlohmann::json post = nlohmann::json();
		nlohmann::json uploaded_files = nlohmann::json::array();
		nlohmann::json query_param = nlohmann::json();

		std::unordered_map<std::string,std::string> cookie;

		//
		// CONNECTION
		//

		int status = 0;
		std::string url;
		FCGX_Request *req;
		std::string content;

		nlohmann::json json()
		{
			nlohmann::json tmp;
			tmp["request_method"] = this->request_method;	
			tmp["script_name"] = this->script_name;	
			tmp["content_type"] = this->content_type;	
			tmp["content_length"] = this->content_length;	
			tmp["redirect_status"] = this->redirect_status;	

			tmp["server_software"] = this->server_software;	
			tmp["server_name"] = this->server_name;	
			tmp["gateway_interface"] = this->gateway_interface;	
			tmp["server_protocol"] = this->server_protocol;	
			tmp["server_port"] = this->server_port;	

			tmp["remote_addr"] = this->remote_addr;	
			tmp["http_accept"] = this->http_accept;	
			tmp["http_user_agent"] = this->http_user_agent;	
			tmp["http_referer"] = this->http_referer;	
			tmp["http_origin"] = this->http_origin;	
			tmp["http_authorization"] = this->http_authorization;

			tmp["post"] = this->post;
			tmp["uploaded_files"] = this->uploaded_files;

			return std::move(tmp);
		};
};


#endif 
