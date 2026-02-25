#include "LISTENER.h"

////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// IMPORT                                                                                 //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

#define ACTOR_TYPE "HTTP::LISTENER"


extern "C" tegia::actors::type_base_t * _init_type(const std::string &type_name)
{	
	auto type = new tegia::actors::type_t<HTTP::LISTENER>(ACTOR_TYPE);

	ADD_ACTION("/init",       &HTTP::LISTENER::init,       ROLES::SESSION::SYSTEM);
	ADD_ACTION("/unload",     &HTTP::LISTENER::unload,     ROLES::SESSION::SYSTEM, ROLES::SESSION::PUBLIC, ROLES::SESSION::USER);
	ADD_ACTION("/domain/add", &HTTP::LISTENER::add_domain, ROLES::SESSION::SYSTEM);

	return type;
};


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// PROVIDE FUNCTION                                                                       //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "libfcgi.lib")

// https://github.com/toshic/libfcgi/blob/master/examples/echo-cpp.cpp

static const unsigned long STDIN_MAX = 1000000;


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// CONSTRUCTOR / DESTRUCTOR                                                               //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////

namespace HTTP {


LISTENER::LISTENER(const std::string &name): tegia::actors::actor_t(ACTOR_TYPE,name)
{
	//
	// Инициализируем JSON SCHEME VALIDATOR для /init
	//

	auto res = this->_validator_init.load(
		R"({
				"$schema": "http://json-schema.org/draft-07/schema#",
				"title": "HTTP config schema",
				"type": "object",
				"properties": 
				{
					"fcgi":
					{
						"type": "object",
						"properties":
						{
							"host":
							{
								"type": "string",
								"description": "Порт, на который приходит fcgi соединение от nginx"
							},
							"listen_queue_backlog":
							{
								"type": "number",
								"description": ""
							}
						},
						"required": ["host","listen_queue_backlog"]
					}
				},
				"required": ["fcgi"]
			})"_json);

	if(res == false)
	{
		std::cout << _ERR_TEXT_ << "LOAD _validator_init ERROR" << std::endl;
		exit(0);
	}

	this->status = 200;
};

LISTENER::~LISTENER() { };

}	// END namespace HTTP


////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// ACTION FUNCTIONS                                                                       //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////


#include "actions/init.cpp"
#include "actions/unload.cpp"
#include "actions/add_domain.cpp"

	
////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                        //
// COMMON FUNCTIONS                                                                       //
//                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////


#include "support/init_port.cpp"


