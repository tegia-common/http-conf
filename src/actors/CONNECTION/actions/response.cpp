#include "../CONNECTION.h"

///
#undef _LOG_LEVEL_
#define _LOG_LEVEL_ _LOG_ERROR_
#include <tegia/context/log.h>
///


////////////////////////////////////////////////////////////////////////////////////////////
/**

	\brief Отправка http-запроса клиенту
	\detail

		
*/   
////////////////////////////////////////////////////////////////////////////////////////////

namespace HTTP {


int CONNECTION::response(const std::shared_ptr<message_t> &message)
{
	int _STATUS_ = 200;
	/////////////////////////////////////////////////////////////////////////////////////////  


	/*
	std::cout << _YELLOW_ << "message->http" << _BASE_TEXT_ << std::endl;
	std::cout << message->http << std::endl;
	std::cout << _YELLOW_ << "message->data" << _BASE_TEXT_ << std::endl;
	std::cout << message->data << std::endl;
	*/

	/*
	std::cout << "HTTP::CONNECTION::response()" << std::endl;
	std::cout << "SCRIPT_NAME     = " << this->request.script_name << std::endl;
	std::cout << "SERVER_NAME     = " << this->request.server_name << std::endl;
	std::cout << "HTTP_REFERER    = " << this->request.http_referer << std::endl;
	std::cout << "HTTP_USER_AGENT = " << this->request.http_user_agent << std::endl;
	std::cout << "HTTP_ORIGIN     = " << this->request.http_origin << std::endl;
	*/


	/////////////////////////////////////////////////////////////////////////////////////////
	//
	// Настраиваем заголовки для CORS
	//
	/////////////////////////////////////////////////////////////////////////////////////////

	/*
	if(this->params->cors.alloworigin == true)
	{
		std::string origin = "";
			
		if(message->http->request.http_origin != "")
		{
			origin = message->http->request.http_origin;
		}
		else
		{
			origin = message->http->request.server_name;
			origin = "http://" + origin;
		}

		std::string header = message->data["responce"]["header"].get<std::string>();   
		header = header + "Access-Control-Allow-Origin: " + origin + "\r\n";
		header = header + "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
		header = header + "Access-Control-Allow-Credentials: true\r\n";
		header = header + "Access-Control-Allow-Headers: DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range\r\n";
		message->data["responce"]["header"] = header;
	}
	*/

	// message->data["response"]["status"] = 200;
	// message->data["response"]["type"] = "application/json";

	message->http["response"]["header"] = "";
	
	/*
	message->data["response"]["data"]["script_name"] = this->request.script_name;
	message->data["response"]["data"]["server_name"] = this->request.server_name;
	message->data["response"]["data"]["http_referer"] = this->request.http_referer;
	message->data["response"]["data"]["http_user_agent"] = this->request.http_user_agent;
	message->data["response"]["data"]["http_origin"] = this->request.http_origin;
	*/

	// message->data["http"]["response"]["data"]["request"] = this->connection->json();

	message->data["connection"] = this->name;
	message->callback.add("http/listener","/unload");

	std::string cookie = "";

	auto _case = tegia::crypt::crc32(
		core::cast<std::string>(message->http["response"]["status"].get<int>()) + 
		message->http["response"]["type"].get<std::string>()
	);

	switch(_case)
	{
		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			200 application/json
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////
		

		case 1492069240:
		{
			this->connection->status = 200;
			this->connection->content = cookie + 
				"Status: 200 OK\r\n"
				"Cache-Control: no-cache\r\n"
				"Content-Type: application/json; charset=utf-8\r\n" + 
				message->http["response"]["header"].get<std::string>() +
				"\r\n" +
				message->data.dump() +
				"\r\n";					
		
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 200;
		}
		break;

		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			400 application/json
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////

		case 3308462187:
		{
			this->connection->content = cookie +
					"Status: 400 Bad Request\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 400;
		}
		break;

		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			403 application/json
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////


		case 2527689711:
		{
			this->connection->content = cookie +
					"Status: 403 Forbidden\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 403;
		}
		break;


		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			404 application/json
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////


		case 3057321892:
		{
			this->connection->content = cookie +
					"Status: 404 Not Found\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 404;
		}
		break;

		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			415 Unsupported Media Type
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////

		case 3738139142:
		{
			this->connection->content = cookie +
					"Status: 415 Unsupported Media Type\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 415;
		}
		break;

		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			434 application/json

			Этот код возникает в сиутации, когда приходит запрос на незарегистрированный 
			в системе домен
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////


		case 1555946182:
		{
			this->connection->content = cookie +
					"Status: 434 Requested Host Unavailable\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);

			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 434;
		}
		break;

		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			500 application/json
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////


		case 315727923:
		{
			this->connection->content = cookie +
					"Status: 500 Internal Server Error\r\n" 
					"Content-Type: application/json; charset=utf-8\r\n"
					"Cache-Control: no-cache\r\n" +
					message->http["response"]["header"].get<std::string>() +
					"\r\n" +
					message->data.dump() +
					"\r\n";
			
			LDEBUG("CONNECTION " + this->name + "\n\n" + this->connection->content);
			
			FCGX_PutStr(this->connection->content.c_str(), this->connection->content.size(),this->connection->req->out);
			FCGX_Finish_r(this->connection->req);
			return 500;
		}
		break;


		//////////////////////////////////////////////////////////////////////////////////////////////
		/*
			DEFAULT
		*/
		//////////////////////////////////////////////////////////////////////////////////////////////


		default:
		{
			std::cout << "_case   = " << _case << std::endl;
			std::cout << "status  = " << message->http["response"]["status"].get<int>() << std::endl;
			std::cout << "type    = " << message->http["response"]["type"].get<std::string>() << std::endl;
			exit(0);
		}
		break;

	}	// END switch(_case)




	/////////////////////////////////////////////////////////////////////////////////////////  
	return _STATUS_;
};

}	// END namespace HTTP
