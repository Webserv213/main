#ifndef SERVER_HPP
# define SERVER_HPP
# include "Location.hpp"
# include <iostream>
# include <map>
# include <string>
# include <vector>
# include <sstream>

class Server
{
private:
    std::string                 root_;
    bool                        autoindex_;
    unsigned long long          client_max_body_size_;
    std::map<std::string, std::string>  error_page_;
    unsigned short              listen_port_;
    std::string                 server_name_;
    std::vector<Location>       location_block_;

public:
    Server();
    ~Server();

    //getter
    std::string getRoot();
    bool getAutoIndex();
    unsigned long long getClientMaxBodySize();
    std::string getErrorPage(std::string error_code);
    unsigned short getListenPort();
    std::string getServerName();
    Location getLocationBlock(int idx);

    //setter
    void setLocationValue(std::string &str, Location &new_location);
    void setRoot(std::string &root);
    void setAutoIndex(bool flag);
    void setClientMaxBodySize(unsigned long long client_max_body_size);
    void setErrorPage(std::string error_code, std::string &path);
    void setListenPort(std::string &listen_port);
    void setServerName(std::string &server_name);
    void setLocation(Location &location);
    
    // util
    int findLocationBlock(std::string url);
};

#endif
