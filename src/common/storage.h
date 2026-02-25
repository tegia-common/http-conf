#include <tegia/tegia.h>


class storage_t
{
    public:
        storage_t(const std::string &storage_dir, const std::string &wsid);
        ~storage_t() = default;

        int save(nlohmann::json &metadata);

    private:
        std::filesystem::path storage_dir;
        std::filesystem::path ws_dir;
        std::string wsid = "";
};


//
//
//

storage_t::storage_t(const std::string &storage_dir, const std::string &wsid)
:storage_dir(storage_dir), wsid(wsid)
{
	if(std::filesystem::exists(this->storage_dir) == false)
	{
		std::filesystem::create_directory(this->storage_dir);
	}

	this->ws_dir = this->storage_dir.string() + "/" + wsid;
	if(std::filesystem::exists(this->ws_dir) == false)
	{
		std::filesystem::create_directory(this->ws_dir);
	}

    std::cout << "storage str  = " << storage_dir << std::endl;
    std::cout << "storage path = " << this->storage_dir.string() << std::endl;
    std::cout << "ws path      = " << this->ws_dir.string() << std::endl;
};

//
//
//

int storage_t::save(nlohmann::json &metadata)
{
    std::string uuid = tegia::random::uuid();
    std::filesystem::path source = metadata["path"].get<std::string>();
    std::filesystem::path destination = this->ws_dir.string() + "/" + uuid + ".file";

    // TODO: Сделать обработку ошибок
    
    //
    // COPY
    //

    {
        std::error_code ec;
        bool result = std::filesystem::copy_file(
            source, 
            destination, 
            std::filesystem::copy_options::overwrite_existing,
            ec
        );

        if(result)
        {
            // TODO: ERROR
        }
    }

    //
    // REMOVE
    //

    {
        std::error_code ec;
        bool result = std::filesystem::remove(source, ec);
    }

    metadata["uuid"] = uuid;
    metadata["path"] = destination.string();
    
    std::cout << metadata << std::endl;
    return 0;
};