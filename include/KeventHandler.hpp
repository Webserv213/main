#ifndef KEVENT_HANDLER_HPP
# define KEVENT_HANDLER_HPP

# include <iostream>
# include <map>
# include <vector>
# include <sys/event.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <fcntl.h>
# include <unistd.h>
# include <sstream>
# include <ctime>

# include "EventRecorder.hpp"
# include "Http.hpp"
# include "Utils.hpp"

# define EVENT_LIST_SIZE 100

enum EventType
{
    ERROR,
    IS_SERVER_SOCKET,
    READ_ERROR,
    READ_CONTINUE,
    READ_FINISH_REQUEST,
    READ_FINISH_FILE,
    SEND_RESPONSE,
    EDIT_FILE,
    CLOSE_CONNECTION,
    IDLE,
    WRITE_CGI,
    READ_CGI
};

enum EventRecorderFlag
{
    ISREQUEST,
    ISFILE,
    RESPONSE_READY
};

class KeventHandler
{
private:
    Http                                http_;
    std::vector<uintptr_t>              server_sockets_;
    std::map<int, EventRecorder>        fd_manager_;
    std::map<int, std::vector<char> >   fd_content_;
    std::map<std::string, std::string>  mime_type_;
    std::vector<struct kevent>          change_list_;
    std::vector<char>   chunked_length_temp_;
    
    // event_list의 크기 생각해보기
    struct kevent                       event_list_[EVENT_LIST_SIZE];
    int                                 kq_;

    void    setMimeType();
    void    disconnectClient(int client_fd);
    void    changeEvents(std::vector<struct kevent>& change_list, uintptr_t ident, int16_t filter,
                        uint16_t flags, uint32_t fflags, intptr_t data, void *udata);
    void    initKqueue();
    int     getServerIndex(Request req);
    void    createRequest(struct kevent* curr_event);
    void    createResponse(struct kevent* curr_event);
    void    createResponseAutoindex(int curr_event_fd, std::string file_path);
    void    sendResponse(struct kevent* curr_event);
    void    setServerSocket(struct sockaddr_in *server_addr, Server server);
    bool    createClientSocket(struct kevent* curr_event);
    int     getEventFlag(struct kevent* curr_event, char *buf, int *n);
    void    socketError(struct kevent*  curr_event);
    void    clientReadError(struct kevent* curr_event);
    void    addContent(struct kevent* curr_event, char buf[], int n);
    bool    isSocket(struct kevent* curr_event);
    int     readFdFlag(struct kevent* curr_event, char *buf, int *n);
    void    createFile(struct kevent* curr_event);
    int     writeFdFlag(struct kevent* curr_event);

    bool    checkAccessMethod(std::string method, Location location);
    int     getLocationIndex(std::vector<std::string> request_target, Server &server, size_t *size);
    void    methodGetHandler(Server &server, Request &req, int curr_event_fd, int loc_idx, size_t size);
    void    methodPostHandler(Server &server, Request &req, int curr_event_fd, int loc_idx, size_t size);
    void    methodDeleteHandler(Server &server, Request &req, int curr_event_fd, int loc_idx, size_t size);
    void    executeCgi(struct kevent* curr_event);
    void    readCgi(struct kevent* curr_event);

    int     compareLocation(std::vector<std::string> t, std::vector<std::string> loc);
    std::string makeDirList(std::string file_path);

    //event utils
    int     addSegmentReqAndReadMode(struct kevent* curr_event, char buf[], int n);

    //req parsing utils
    void    parsingReqStartLineAndHeaders(struct kevent* curr_event);

    // method_error
    void notFound404(int curr_event_fd);
    void notAllowedMethod405(int curr_event_fd);

    // method_utils
    void setReadFileEvent(int curr_event_fd, int fd);

    // post_utils
    std::string createFilePath(Server &server, Request &req, int loc_idx, size_t size);
    void createFileForPost(int curr_event_fd, std::string file_path);

    // get_utils
    void getFaviconFile(int curr_event_fd);
    void addFileName_getFileFd(std::string file_path, Server &server, int loc_idx, int curr_event_fd);
    bool checkCgi(std::vector<std::string> loc, std::string extension);

public:
    KeventHandler(Http &http);
    ~KeventHandler();
    void openListenSocket();
    void runServer();
};

# endif
