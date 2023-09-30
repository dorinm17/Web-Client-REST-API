#ifndef _CLIENT_
#define _CLIENT_

#define HOST "34.254.242.81"
#define PORT 8080

#define JSON_APP "application/json"

#define REG_URL "/api/v1/tema/auth/register"
#define LOGIN_URL "/api/v1/tema/auth/login"
#define LIB_URL "/api/v1/tema/library/access"
#define BOOKS_URL "/api/v1/tema/library/books"
#define LOGOUT_URL "/api/v1/tema/auth/logout"

void register_user(int sockfd);
void login(int sockfd, char *cookie);
void enter_library(int sockfd, char *cookie, char *token);
void get_books(int sockfd, char *cookie, char *token);
void get_book(int sockfd, char *cookie, char *token);
void add_book(int sockfd, char *cookie, char *token);
void delete_book(int sockfd, char *cookie, char *token);
void logout(int sockfd, char *cookie, char *token);

#endif
