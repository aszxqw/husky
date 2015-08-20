#ifndef HUSKY_WORKER_HPP
#define HUSKY_WORKER_HPP

#include "limonp/ThreadPool.hpp"
#include "irequest_handler.h"
#include "net_util.h"

namespace husky {
const char* const CLIENT_IP_K = "CLIENT_IP";
const size_t RECV_BUFFER_SIZE = 16 * 1024;

const struct linger LNG = {1, 1};
const struct timeval SOCKET_TIMEOUT = {16, 0};

class WorkerThread: public ITask {
 public:
  WorkerThread(int sockfs, IRequestHandler& reqHandler):
    sockfd_(sockfs), req_handler_(reqHandler) {
  }
  virtual ~WorkerThread() {
  }

  virtual void run() {
    do {
      if(!SetSockopt(sockfd_)) {
        LogError("_getsockopt failed.");
        break;
      }
      string strSnd, strRetByHandler;
      HttpReqInfo httpReq;
      if(!Receive(sockfd_, httpReq)) {
        LogError("Receive failed.");
        break;
      }

      if(httpReq.IsGET() && !req_handler_.DoGET(httpReq, strRetByHandler)) {
        LogError("DoGET failed.");
        break;
      }
      if(httpReq.IsPOST() && !req_handler_.DoPOST(httpReq, strRetByHandler)) {
        LogError("DoPOST failed.");
        break;
      }
      strSnd = string_format(HTTP_FORMAT, CHARSET_UTF8, strRetByHandler.length(), strRetByHandler.c_str());

      if(!Send(sockfd_, strSnd)) {
        LogError("Send failed.");
        break;
      }
    } while(false);


    if(-1 == close(sockfd_)) {
      LogError(strerror(errno));
    }
  }
 private:
  bool Receive(int sockfd, HttpReqInfo& httpInfo) const {
    char recvBuf[RECV_BUFFER_SIZE];
    int n = 0;
    while(!httpInfo.IsBodyFinished() && (n = recv(sockfd, recvBuf, RECV_BUFFER_SIZE, 0)) > 0) {
      if(!httpInfo.IsHeaderFinished()) {
        if(!httpInfo.ParseHeader(recvBuf, n)) {
          LogError("ParseHeader failed. ");
          return false;
        }
        continue;
      }
      httpInfo.AppendBody(recvBuf, n);
    }
    if(n < 0) {
      LogError(strerror(errno));
      return false;
    }
    return true;
  }
  bool Send(int sockfd, const string& strSnd) const {
    if(-1 == send(sockfd, strSnd.c_str(), strSnd.length(), 0)) {
      LogError(strerror(errno));
      return false;
    }
    return true;
  }
  bool SetSockopt(int sockfd) const {
    if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&LNG, sizeof(LNG))) {
      LogError(strerror(errno));
      return false;
    }
    if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&SOCKET_TIMEOUT, sizeof(SOCKET_TIMEOUT))) {
      LogError(strerror(errno));
      return false;
    }
    if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&SOCKET_TIMEOUT, sizeof(SOCKET_TIMEOUT))) {
      LogError(strerror(errno));
      return false;
    }
    return true;
  }

  int sockfd_;
  IRequestHandler& req_handler_;
};
}

#endif
