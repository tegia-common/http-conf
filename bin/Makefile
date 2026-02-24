include ./Makefile.variable

_OK_=\033[32;2m[OK]\033[0m  

# =============================================================================================================
#
# BUILD
#
# =============================================================================================================

http:
	@echo " "
	@echo "------------------------------------------------------------"
	@echo "HTTP CONFIGURATION: \033[32;1m BUILD \033[0m"
	@echo "------------------------------------------------------------"
	@echo " "

	make libLISTENER
	make libCONNECTION
#	make libA2Session

install:
	@echo " "
	@echo "------------------------------------------------------------"
	@echo "HTTP CONFIGURATION: \033[32;1m INSTALL \033[0m"
	@echo "------------------------------------------------------------"
	@echo " "

	bash ./install.sh
	make

	@echo " "
	@echo "${_OK_}configuration 'http-conf' successfully installed"
	@echo " "

# =============================================================================================================
#
# BUILD ACTORS
#
# =============================================================================================================

libLISTENER:
	g++ -c -o FCgiIO.o              ../src/actors/LISTENER/common/FCgiIO.cpp    $(Flag) -fPIC
	g++ -c -o LISTENER.o            ../src/actors/LISTENER/LISTENER.cpp         $(Flag) -fPIC
	g++ -c -o domain.o              ../src/common/domain.cpp                    $(Flag) -fPIC
	g++ -c -o connection.o          ../src/common/connection.cpp                $(Flag) -fPIC
	g++ -shared -fPIC -o libLISTENER.so             \
	     FCgiIO.o domain.o connection.o               \
	     LISTENER.o -L/usr/lib -lcgicc -lfcgi -lfcgi++ -lnlohmann_json_schema_validator $(Flag)


libCONNECTION:
	g++ -c -o FCgiIO.o              ../src/actors/LISTENER/common/FCgiIO.cpp    $(Flag) -fPIC
	g++ -c -o CONNECTION.o          ../src/actors/CONNECTION/CONNECTION.cpp -I../vendors/cpp-jwt/include/ $(Flag) -fPIC
	g++ -c -o domain.o              ../src/common/domain.cpp                    $(Flag) -fPIC
	g++ -c -o connection.o          ../src/common/connection.cpp                $(Flag) -fPIC
	g++ -shared -fPIC -o libCONNECTION.so     \
		FCgiIO.o domain.o connection.o \
	    CONNECTION.o -L/usr/lib -lcgicc -lfcgi -lfcgi++ -lnlohmann_json_schema_validator $(Flag)




libA2Sessions:
	g++ -c -o A2Sessions.o      ../actors/A2Sessions/A2Sessions.cpp -I../vendors/cpp-jwt/include/ $(Flag) -fPIC
	g++ -shared -fPIC -o libA2Sessions.so     \
	     A2Sessions.o -lstdc++fs $(Flag)		 
