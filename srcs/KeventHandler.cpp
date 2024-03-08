#include "KeventHandler.hpp"

KeventHandler::KeventHandler(Http &http): http_(http)
{

}

KeventHandler::~KeventHandler()
{

}

void    checkRequest(Request &req)
{
    // std::cout << "=====start_line=====\n";
    std::cout << "Host: " << req.getHeaders().getHost() << "\n";
    std::cout << "Listen socket: " << req.getHeaders().getListenPort() << "\n";
}

void    KeventHandler::disconnectClient(int client_fd)
{
    std::cout << "client disconnected: " << client_fd << std::endl;
    close(client_fd);
    fd_manager_.erase(client_fd);
    fd_content_.erase(client_fd);
}

void KeventHandler::changeEvents(std::vector<struct kevent>& change_list, uintptr_t ident, int16_t filter,
        uint16_t flags, uint32_t fflags, intptr_t data, void *udata)
{
    struct kevent temp_event;

    EV_SET(&temp_event, ident, filter, flags, fflags, data, udata);
    change_list.push_back(temp_event);
}

void KeventHandler::setServerSocket(struct sockaddr_in *server_addr, Server server)
{
    memset(&(*server_addr), 0, sizeof(*server_addr));
    (*server_addr).sin_family = AF_INET;
    (*server_addr).sin_addr.s_addr = htonl(INADDR_ANY);
    (*server_addr).sin_port = htons(server.getListenPort());
}

void KeventHandler::openListenSocket()
{
    int listen_socket_fd;
    std::vector<Server> server;
    struct sockaddr_in server_addr;

    server = http_.getServer();

    for(size_t i = 0; i < server.size(); i++)
    {
        setServerSocket(&server_addr, server[i]);

        listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
        if (listen_socket_fd == -1)
            throw(std::runtime_error("socket() error\n"));

        if (bind(listen_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
            throw(std::runtime_error("bind() error\n"));

        if (listen(listen_socket_fd, 50) == -1)
            throw(std::runtime_error("listen() error\n"));
        fcntl(listen_socket_fd, F_SETFL, O_NONBLOCK);

        server_sockets_.push_back(listen_socket_fd);
        changeEvents(change_list_, listen_socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);

        // using debug
        std::cout << "listen_socket_fd : " << listen_socket_fd << ", webserv on" << std::endl;
    }
}

void KeventHandler::initKqueue()
{
    kq_ = kqueue();
    if (kq_ == -1)
        throw(std::runtime_error("kqueue() error\n"));
}

bool    KeventHandler::createClientSocket(struct kevent* curr_event)
{
    bool result = false;

    for(size_t i = 0; i < server_sockets_.size(); i++)
    {
        if (curr_event->ident == server_sockets_[i])
        {
            int client_socket = accept(server_sockets_[i], NULL, NULL);
            if (client_socket == -1)
                throw(std::runtime_error("accept() error\n"));
            std::cout << "accept new client: " << client_socket << std::endl;
            fcntl(client_socket, F_SETFL, O_NONBLOCK);

            /* add event for client socket - add read & write event */
            EventRecorder st;
            fd_manager_[client_socket] = st;
            changeEvents(change_list_, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd_manager_[client_socket]);
            changeEvents(change_list_, client_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, &fd_manager_[client_socket]);
            fd_content_[client_socket];
            result = true;
        }
    }
    return (result);
}

int KeventHandler::getServerIndex(Request req)
{
    // std::cout << req.getHeaders().getListenPort() << " " << req.getHeaders().getHost() << "\n";
    for (size_t i = 0; i < http_.getServer().size(); i++)
    {
        // std::cout << "=======get_server" << "\n";
        // std::cout << http_.getServer()[i].getListenPort() << "\n" << http_.getServer()[i].getServerName() << "\n";
        if (req.getHeaders().getListenPort() == http_.getServer()[i].getListenPort() &&
            req.getHeaders().getHost() == http_.getServer()[i].getServerName())
        {
            // std::cout << http_.getServer()[i].getListenPort() << "\n" << http_.getServer()[i].getServerName() << "\n";
            return (i);
        }
    }
    return (0);
}

int KeventHandler::compareLocation(std::vector<std::string> t, std::vector<std::string> loc)
{
    size_t i = 0;

    while (i < t.size())
    {
        if (i > loc.size() - 1)
            return (i);

        if (t[i] != loc[i])
            return (i);
        i++;
    }
    return (i);
}

int KeventHandler::getLocationIndex(std::vector<std::string> request_target, Server &server)
{
    // target 
    // 슬래시 기준으로 잘라서 vector에 삽입

    // post_fix 슬래시 시준으로 잘라서 vector에 삽입
    // target하고 postfix 벡터 비교하면서 하나라고 다른게 있으면 바로 끝 아니면 인덱스 증가 (제일 큰 인덱스로 계속 갱신)

    int index = 0;
    size_t size, size2;
    std::string url;

    // 예외! size와 target.size() 무조건 걔의 인덱스 반환
    size = compareLocation(request_target, server.getLocation()[0].getUrlPostfix());           // 초기값
    for(size_t i = 1; i < server.getLocation().size(); i++)
    {
        size2 = compareLocation(request_target, server.getLocation()[i].getUrlPostfix());
        if (size2 == server.getLocation()[i].getUrlPostfix().size())
            return (i);
        if (size < size2)
        {
            size = size2;
            index = i;
        }
    }
    return (index);
}

void KeventHandler::methodGetHandler(Server &server, Request &req, int curr_event_fd)
{
    std::string file_path;
    if (req.getRequestLine().getRequestTarget()[0] == "/favicon.ico")
    {
        file_path = "./var/www/favicon.ico";
    }
    else
    {
        int loc_idx;
    
        loc_idx = getLocationIndex(req.getRequestLine().getRequestTarget(), server);
        // file_path = server.getRoot() + req.getRequestLine().getRequestTarget() + "/index.html";
        file_path = server.getLocationBlock(loc_idx).getRoot();
        for (size_t i = 0; i < req.getRequestLine().getRequestTarget().size(); i++)
            file_path += req.getRequestLine().getRequestTarget()[i];
        file_path += "/index.html";
        // file_path = server.getLocationBlock(loc_idx).getRoot() + req.getRequestLine().getRequestTarget() + "/index.html";
    }

    // location
    // 1. target 확인해서 locaion 길이 제일 맞는거
    // 2. 디렉토리 -> autoindex on -> autoindex
    // 3. 디렉토리 -> autoindex off -> index.html
    // 4. 아니면 404


    int fd;

    fd = open (file_path.c_str(), O_RDONLY);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if (fd < 0)
    {
        std::cout << file_path + " fd_error : " << fd << std::endl;
        exit(0);
    }
    fd_content_[fd];
    EventRecorder event_recorder(curr_event_fd);
    event_recorder.setEventReadFile(1);
    fd_manager_[fd] = event_recorder;
    changeEvents(change_list_, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd_manager_[fd]);
    changeEvents(change_list_, curr_event_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    fd_manager_[curr_event_fd].setEventWriteRes(-1);
}

void    KeventHandler::createRequest(struct kevent* curr_event)
{
    Request req;

    std::string buf;
    std::istringstream streamLine(charVectorToString(fd_content_[curr_event->ident]));

    std::getline(streamLine, buf, ' ');
    req.getRequestLine().setMethod(buf);
    std::getline(streamLine, buf, ' ');
    req.getRequestLine().setRequestTarget(buf);
    std::getline(streamLine, buf);
    req.getRequestLine().setVersion(buf);

    while (std::getline(streamLine, buf, ' '))   //Request Header
    {
        if (buf == "Host:") {
            std::getline(streamLine, buf);
            req.getHeaders().setFullPath(buf);
        }
        else if (buf == "Accept:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setAccept(buf);
        }
        else if (buf == "Accept-Encoding:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setAcceptEncoding(buf);
        }
        else if (buf == "Accept-Language:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setAcceptLanguage(buf);
        }
        else if (buf == "Connection:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setConnection(buf);
        }
        else if (buf == "Upgrade-Insecure-Requests:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setUpgradeInsecureRequests(buf);
        }
        else if (buf == "User-Agent:")
        {
            std::getline(streamLine, buf);
            req.getHeaders().setUserAgent(buf);
        }
    }
    fd_manager_[curr_event->ident].setRequest(req);

    std::string url;
    int index = getServerIndex(req);

    if (req.getRequestLine().getMethod() == "GET")
    {
        methodGetHandler(http_.getServer()[index], req, curr_event->ident);
    }
    else if (req.getRequestLine().getMethod() == "POST")
    {
        ;
    }
}

void KeventHandler::createResponse(struct kevent* curr_event)
{
    int parent_fd = fd_manager_[curr_event->ident].getParentClientFd();
    std::string version = fd_manager_[parent_fd].getRequest().getRequestLine().getVersion();

    fd_manager_[parent_fd].getResponse().getStatusLine().setVersion(version);
    fd_manager_[parent_fd].getResponse().getStatusLine().setStatusCode("200");
    fd_manager_[parent_fd].getResponse().getStatusLine().setStatusText("OK");
    fd_manager_[parent_fd].getResponse().getHeaders().setServer("default");
    fd_manager_[parent_fd].getResponse().getHeaders().setKeepAlive("timeout=100");

    time_t rawTime;
    time(&rawTime);

    struct tm *timeInfo;
    timeInfo = gmtime(&rawTime);

    char buffer[80];
    strftime(buffer, 80, "%a, %d %b %Y %H:%M:%S GMT", timeInfo);

    fd_manager_[parent_fd].getResponse().getHeaders().setDate(buffer);

    if (fd_manager_[parent_fd].getRequest().getRequestLine().getRequestTarget()[0] != "/favicon.ico")
        fd_manager_[parent_fd].getResponse().getHeaders().setContentType("text/html");
    else
        fd_manager_[parent_fd].getResponse().getHeaders().setContentType("image/x-icon");

    fd_manager_[parent_fd].getResponse().getHeaders().setLastModified("default");
    fd_manager_[parent_fd].getResponse().getHeaders().setTransferEncoding("default");
    fd_manager_[parent_fd].getResponse().getHeaders().setConnection("keep-alive");
    fd_manager_[parent_fd].getResponse().getHeaders().setContentEncoding("default");

    if (fd_manager_[parent_fd].getRequest().getRequestLine().getRequestTarget()[0] != "/favicon.ico")
        fd_manager_[parent_fd].getResponse().getHeaders().setContentLength(std::to_string(fd_content_[curr_event->ident].size()));
    else
        fd_manager_[parent_fd].getResponse().getHeaders().setContentLength(std::to_string(1150));

    std::string file_data(charVectorToString(fd_content_[curr_event->ident]));
    fd_manager_[parent_fd].getResponse().setBody(file_data);

    std::string res_tmp;
    res_tmp = "";
    fd_manager_[parent_fd].getResponse().getStatusLine().setVersion("HTTP/1.1");
    res_tmp += (fd_manager_[parent_fd].getResponse().getStatusLine().getVersion() + " ");
    res_tmp += (fd_manager_[parent_fd].getResponse().getStatusLine().getStatusCode() + " ");
    res_tmp += (fd_manager_[parent_fd].getResponse().getStatusLine().getStatusText() + "\r\n");
    res_tmp += ("Date: " + fd_manager_[parent_fd].getResponse().getHeaders().getDate() + "\n");
    res_tmp += ("Content-Type: " + fd_manager_[parent_fd].getResponse().getHeaders().getContentType() + "\n");
    res_tmp += ("Content-Length: " + fd_manager_[parent_fd].getResponse().getHeaders().getContentLength() + "\n");
    res_tmp += ("Connection: " + fd_manager_[parent_fd].getResponse().getHeaders().getConnection() + "\n");
    res_tmp += ("Keep-Alive: " + fd_manager_[parent_fd].getResponse().getHeaders().getKeepAlive() + "\r\n");
    res_tmp += "\r\n" + charVectorToString(fd_manager_[parent_fd].getResponse().getBody());
    fd_content_[parent_fd] = stringToCharVector(res_tmp);

    fd_manager_[parent_fd].setEventWriteRes(1);
    close(curr_event->ident);
    fd_content_.erase(curr_event->ident);
    fd_manager_.erase(curr_event->ident);
    changeEvents(change_list_, parent_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, &fd_manager_[parent_fd]);
}

void    KeventHandler::sendResponse(struct kevent* curr_event)
{
    int n = write(curr_event->ident, &(*fd_content_[curr_event->ident].begin()), fd_content_[curr_event->ident].size());
    if (n == -1)
    {
        std::cerr << "client write error!" << std::endl;
        disconnectClient(curr_event->ident);
        fd_manager_[curr_event->ident].setFdError(1);
    }
    else
    {
        fd_content_[curr_event->ident].clear();
        fd_manager_[curr_event->ident].setEventWriteRes(-1);
        changeEvents(change_list_, curr_event->ident, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        changeEvents(change_list_, curr_event->ident, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd_manager_[curr_event->ident]);
        EventRecorder n;
        fd_manager_[curr_event->ident] = n;
    }
}

bool KeventHandler::isSocket(struct kevent* curr_event)
{
     for(size_t i = 0; i < server_sockets_.size(); i++)
    {
        if (curr_event->ident == server_sockets_[i])
            return (true);
    }
    return (false);
}

int KeventHandler::readFdFlag(struct kevent* curr_event, char *buf, int *n)
{
    if (fd_content_.find(curr_event->ident) != fd_content_.end())
    {
        memset(buf, 0, BUFFER_SIZE);
        *n = read(curr_event->ident, buf, BUFFER_SIZE);
        buf[*n] = '\0';

        if (*n < 0)
            return (READ_ERROR);
        else if (curr_event->data == *n)
        {
            addContent(curr_event, buf, *n);
            if (fd_manager_[curr_event->ident].getEventReadFile() == -1)
            {
                if (fd_content_[curr_event->ident].size() != 0)
                    return (READ_FINISH_REQUEST);
                
            }
            else
                return (READ_FINISH_FILE);
        }
        else
            return (READ_CONTINUE);
    }
    if (curr_event->flags & EV_EOF)
        return (CLOSE_CONNECTION);
    return (-1);
}

int  KeventHandler::writeFdFlag(struct kevent* curr_event)
{
    std::map<int, std::vector<char> >::iterator it = fd_content_.find(curr_event->ident);
    if (it != fd_content_.end())
    {
        if (fd_content_[curr_event->ident].size() != 0 && fd_manager_[curr_event->ident].getEventWriteRes() == 1)
            return (SEND_RESPONSE);
        else
            return (EDIT_FILE);
    }
    std::cout << "write : " << curr_event->flags << "\n";
    return (-1);
}

int  KeventHandler::getEventFlag(struct kevent* curr_event, char *buf, int *n)
{
    if (curr_event->flags & EV_ERROR)
        return (ERROR);
    else if (curr_event->filter == EVFILT_READ)
    {
        if (isSocket(curr_event))
            return (IS_SERVER_SOCKET);
        return (readFdFlag(curr_event, buf, n));
    }
    else if (curr_event->filter == EVFILT_WRITE)
        return (writeFdFlag(curr_event));
    std::cout << "ev_flags: " << curr_event->flags << "\n";
    return (-1);
}

void KeventHandler::socketError(struct kevent*  curr_event)
{
    for(size_t i = 0; i < server_sockets_.size(); i++)
    {
        if (curr_event->ident == server_sockets_[i])
            throw(std::runtime_error("server socket error"));
        else
        {
            throw(std::runtime_error("client socket error"));
            disconnectClient(curr_event->ident);
        }
    }
}

void KeventHandler::clientReadError(struct kevent* curr_event)
{
    std::cerr << "client read error!" <<std::endl;
    disconnectClient(curr_event->ident);
}

void KeventHandler::addContent(struct kevent* curr_event, char buf[], int n)
{
    fd_content_[curr_event->ident].insert(fd_content_[curr_event->ident].end(), buf, buf + n);
}

void KeventHandler::runServer(void)
{
    int             new_events;
    int             event_type;
    struct kevent*  curr_event;
    char            buf[BUFFER_SIZE];
    int             n;

    initKqueue();
    while (1)
    {
        new_events = kevent(kq_, &change_list_[0], change_list_.size(), event_list_, EVENT_LIST_SIZE, NULL);
        if (new_events == -1)
            throw(std::runtime_error("kevent() error\n"));

        change_list_.clear();
        for (int i = 0; i < new_events; ++i)
        {
            curr_event = &event_list_[i];
            event_type = getEventFlag(curr_event, buf, &n);

            switch(event_type)
            {
                case ERROR :
                    socketError(curr_event);
                    break ;

                case IS_SERVER_SOCKET :
                    createClientSocket(curr_event);
                    break ;

                case READ_ERROR :
                    clientReadError(curr_event);
                    break ;

                case READ_FINISH_REQUEST :
                    createRequest(curr_event);
                    break ;

                case READ_FINISH_FILE :
                    createResponse(curr_event);
                    break ;

                case READ_CONTINUE :
                    addContent(curr_event, buf, n);
                    break ;

                case SEND_RESPONSE :
                    sendResponse(curr_event);
                    break ;

                case EDIT_FILE :
                    break ;

                case CLOSE_CONNECTION :
                    disconnectClient(curr_event->ident);
                    break ;

                default :
                {
                    std::cout << "flag: " << curr_event->flags << "\n";
                    std::cout << "type: " << event_type << "\n";
                    throw(std::runtime_error("event exception error\n"));
                }
            }
        }
    }
}
