#include "ResponseLine.hpp"

ResponseLine::ResponseLine(void)
{
    version_ = "";
    status_code_ = "";
    status_text_ = "";
}

ResponseLine::~ResponseLine(void)
{
    
}

void    ResponseLine::setVersion(std::string version)
{
    version_ = version;
}

void    ResponseLine::setStatusCode(std::string status_code)
{
    status_code_ = status_code;
}

void    ResponseLine::setStatusText(std::string status_text)
{
    status_text_ = status_text;
}

std::string& ResponseLine::getVersion(void)
{
    return (version_);
}

std::string& ResponseLine::getStatusCode(void)
{
    return (status_code_);
}

std::string& ResponseLine::getStatusText(void)
{
    return (status_text_);
}
