#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"
#include "client.h"

// common instructions for register and login
int reg_login(int sockfd, char *url)
{
    char username[LINELEN] = {0}, password[LINELEN] = {0};

    // read username and password
    printf("username=");
    fgets(username, LINELEN, stdin);
    username[strlen(username) - 1] = '\0';

    printf("password=");
    fgets(password, LINELEN, stdin);
    password[strlen(password) - 1] = '\0';

    if (strchr(username, ' ') || strchr(password, ' ') 
        || !strlen(username) || !strlen(password)) {
        printf("401 - Unauthorized - All fields must be completed and not contain spaces!\n");
        return 1;
    }

    // instantiate JSON object
    JSON_Value *json_value = json_value_init_object();
    JSON_Object *json_object = json_value_get_object(json_value);
    json_object_set_string(json_object, "username", username);
    json_object_set_string(json_object, "password", password);
    char *json_buf = json_serialize_to_string(json_value);

    // send request
    char *request = compute_post_request(HOST, url, JSON_APP, &json_buf,
        1, NULL, 0, NULL);
    send_to_server(sockfd, request);

    // free memory
    free(request);
    json_free_serialized_string(json_buf);
    json_value_free(json_value);

    return 0;
}

void register_user(int sockfd)
{
    // error check
    if (reg_login(sockfd, REG_URL))
        return;

    // check server reply
    char *reply = receive_from_server(sockfd);

    if (strstr(reply, "201 Created"))
        printf("201 - Created - Successfully registered\n");
    else
        printf("401 - Unauthorized - User already exists!\n");

    // free memory
    free(reply);
}

void login(int sockfd, char *cookie)
{
    // error check
    if (reg_login(sockfd, LOGIN_URL))
        return;

    // check server reply
    char *reply = receive_from_server(sockfd);

    if (strstr(reply, "200 OK")) {
        printf("200 - OK - Successfully logged in\n");

        // save cookie
        char *p = strstr(reply, "connect.sid=");
        p = strtok(p, ";");
        strcpy(cookie, p);
    } else {
        printf("401 - Unauthorized - Wrong username or password!\n");
    }

    // free memory
    free(reply);
}

void enter_library(int sockfd, char *cookie, char *token)
{
    if (!strlen(cookie)) {
        printf("401 - Unauthorized - Not logged in!\n");
        return;
    }

    // send request
    char *request = compute_get_request(HOST, LIB_URL, NULL, &cookie, 1, NULL);
    send_to_server(sockfd, request);

    // server reply
    char *reply = receive_from_server(sockfd);
    printf("200 - OK - Granted access to library\n");

    // save JWT token
    JSON_Value *json_value = json_parse_string(
        basic_extract_json_response(reply));
    JSON_Object *json_object = json_value_get_object(json_value);
    strcpy(token, (char *) json_object_get_string(json_object, "token"));

    // free memory
    json_value_free(json_value);
    free(request);
    free(reply);
}

void get_books(int sockfd, char *cookie, char *token)
{
    if (!strlen(token)) {
        printf("403 - Forbidden - Not granted access to library!\n");
        return;
    }

    // send request
    char *request = compute_get_request(HOST, BOOKS_URL, NULL, &cookie, 1,
        token);
    send_to_server(sockfd, request);

    // server reply
    char *reply = receive_from_server(sockfd);
    printf("200 - OK - Successfully received books\n");

    // print books
    char *p = strchr(reply, '[');
    JSON_Value *json_value = json_parse_string(p);
    char *json_buf = json_serialize_to_string(json_value);
    printf("%s\n", json_buf);

    // free memory
    json_value_free(json_value);
    json_free_serialized_string(json_buf);
    free(request);
    free(reply);
}

// common instructions for get_book and delete_book
int get_delete(int sockfd, char *cookie, char *token, int func)
{
    if (!strlen(token)) {
        printf("403 - Forbidden - Not granted access to library!\n");
        return 1;
    }

    // read book id
    char id[LINELEN];
    printf("id=");
    fgets(id, LINELEN, stdin);
    id[strlen(id) - 1] = '\0';

    // check if id is empty or does not contain only digits
    if (!strlen(id) || strspn(id, "0123456789") != strlen(id)) {
        printf("400 - Bad Request - ID must contain only digits!\n");
        return 1;
    }

    // create request URL
    char url[LINELEN];
    strcpy(url, BOOKS_URL);
    strcat(url, "/");
    strcat(url, id);

    // send request
    char *request;

    if (func == 0)
        request = compute_get_request(HOST, url, NULL, &cookie, 1,
            token);
    else
        request = compute_delete_request(HOST, url, NULL, &cookie, 1,
            token);

    send_to_server(sockfd, request);

    // free memory
    free(request);

    return 0;
}

void get_book(int sockfd, char *cookie, char *token)
{
    // error check
    if (get_delete(sockfd, cookie, token, 0))
        return;

    // check server reply
    char *reply = receive_from_server(sockfd);

    if (!strstr(reply, "No book")) {
        printf("200 - OK - Successfully received book\n");

        // print book
        char *p = strchr(reply, '{');
        JSON_Value *json_value = json_parse_string(p);
        char *json_buf = json_serialize_to_string_pretty(json_value);
        printf("%s\n", json_buf);

        // free memory
        json_value_free(json_value);
        json_free_serialized_string(json_buf);
    } else {
        printf("404 - Resource Not Found - Book not in the database!\n");
    }

    // free memory
    free(reply);
}

void add_book(int sockfd, char *cookie, char *token)
{
    if (!strlen(token)) {
        printf("403 - Forbidden - Not granted access to library!\n");
        return;
    }

    // read book information
    char title[LINELEN], author[LINELEN], genre[LINELEN], publisher[LINELEN],
        aux_page_count[LINELEN];
    int page_count;

    printf("title=");
    fgets(title, LINELEN, stdin);
    title[strlen(title) - 1] = '\0';

    printf("author=");
    fgets(author, LINELEN, stdin);
    author[strlen(author) - 1] = '\0';

    printf("genre=");
    fgets(genre, LINELEN, stdin);
    genre[strlen(genre) - 1] = '\0';

    printf("publisher=");
    fgets(publisher, LINELEN, stdin);
    publisher[strlen(publisher) - 1] = '\0';

    printf("page_count=");
    fgets(aux_page_count, LINELEN, stdin);
    aux_page_count[strlen(aux_page_count) - 1] = '\0';

    // check if any field is empty
    if (!strlen(title) || !strlen(author) || !strlen(genre)
        || !strlen(publisher) || !strlen(aux_page_count)) {
        printf("400 - Bad Request - All fields must be filled!\n");
        return;
    }

    // check if page_count does not contain only digits
    if (strspn(aux_page_count, "0123456789") != strlen(aux_page_count)) {
        printf("400 - Bad Request - Page count must contain only digits!\n");
        return;
    }

    page_count = atoi(aux_page_count);

    // instantiate JSON object
    JSON_Value *json_value = json_value_init_object();
    JSON_Object *json_object = json_value_get_object(json_value);
    json_object_set_string(json_object, "title", title);
    json_object_set_string(json_object, "author", author);
    json_object_set_string(json_object, "genre", genre);
    json_object_set_number(json_object, "page_count", page_count);
    json_object_set_string(json_object, "publisher", publisher);
    char *json_buf = json_serialize_to_string(json_value);

    // send request
    char *request = compute_post_request(HOST, BOOKS_URL, JSON_APP, &json_buf,
        1, &cookie, 1, token);
    send_to_server(sockfd, request);

    // server reply
    char *reply = receive_from_server(sockfd);
    printf("201 - Created - Successfully added book\n");

    // free memory
    json_value_free(json_value);
    json_free_serialized_string(json_buf);
    free(request);
    free(reply);
}

void delete_book(int sockfd, char *cookie, char *token)
{
    // error check
    if (get_delete(sockfd, cookie, token, 1))
        return;

    // check server reply
    char *reply = receive_from_server(sockfd);

    if (!strstr(reply, "No book"))
        printf("200 - OK - Successfully deleted book\n");
    else
        printf("404 - Resource Not Found - Book not in the database!\n");

    // free memory
    free(reply);
}

void logout(int sockfd, char *cookie, char *token)
{
    if (!strlen(cookie)) {
        printf("400 - Bad Request - Not logged in!\n");
        return;
    }

    // send request
    char *request = compute_get_request(HOST, LOGOUT_URL, NULL, &cookie, 1,
        NULL);
    send_to_server(sockfd, request);

    // server reply
    char *reply = receive_from_server(sockfd);
    printf("200 - OK - Successfully logged out\n");

    // set cookie and token to empty string
    memset(cookie, 0, LINELEN);
    memset(token, 0, LINELEN);

    // free memory
    free(request);
    free(reply);
}

int main(void)
{
    int sockfd;
    char cmd[LINELEN] = {0}, cookie[LINELEN] = {0}, token[LINELEN] = {0};

    do {
        // read command
        fgets(cmd, LINELEN, stdin);
        cmd[strlen(cmd) - 1] = '\0';

        // open connection with server
        sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

        // handle client commands
        if (strchr(cmd, ' '))
            printf("No spaces in command!\n");
        else if (strcmp(cmd, "register") == 0)
            register_user(sockfd);
        else if (strcmp(cmd, "login") == 0)
            login(sockfd, cookie);
        else if (strcmp(cmd, "enter_library") == 0)
            enter_library(sockfd, cookie, token);
        else if (strcmp(cmd, "get_books") == 0)
            get_books(sockfd, cookie, token);
        else if (strcmp(cmd, "get_book") == 0)
            get_book(sockfd, cookie, token);
        else if (strcmp(cmd, "add_book") == 0)
            add_book(sockfd, cookie, token);
        else if (strcmp(cmd, "delete_book") == 0)
            delete_book(sockfd, cookie, token);
        else if (strcmp(cmd, "logout") == 0)
            logout(sockfd, cookie, token);
        else if (strcmp(cmd, "exit") != 0)
            printf("400 - Bad Request - Unrecognized command!\n");

        // close connection with server
        close_connection(sockfd);
    } while (strcmp(cmd, "exit") != 0);

    return 0;
}
