#include "../includes/G-2301-05-P2-basicCommandsFromServer.h"
#include "../includes/G-2301-05-P2-audio.h"
#include "../includes/G-2301-05-P1-socket.h"

struct threadRecvArgs {
    char* hostname;
    int port;
    char* filename;
    unsigned long length;
    char sender[10];
};

struct threadAudioArgs {
    int port;
    char* hostname;
    char* sender;
};

/**
 * @brief rutina por defecto ante un mensaje no reconocido por este cliente
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactDefault(char*strin) {
    return IRC_OK;
}

/**
 * @brief maneja cualquier mensaje en general que envie el servidor, imprimiendo en xchat strin
 *
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long reactPrint(char*strin) {
    printXchat(NULL, NULL, strin, TRUE);
    return IRC_OK;
}
//##############################################################################################
//##############################################################################################
//------------------------------------------BASICOS---------------------------------------------
//##############################################################################################
//##############################################################################################

/**
 * @brief rutina que maneja la llegada de un comando PASS
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactPass(char* strin) {
    //TODO
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando NICK. Esta rutina cambia el nick del usuario
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactNick(char *strin) {
    char *prefix, *nick, *msg, *nickold, *userold, *host, *server, info[512], *command;
    long int ret;

    prefix = nick = msg = nickold = userold = host = server = NULL;

    if ((ret = IRCParse_Nick(strin, &prefix, &nick, &msg)) != IRC_OK)
        return logIntError(ret, "error @ reactNick -> IRCParse_Nick");
    if ((ret = IRCParse_ComplexUser(prefix, &nickold, &userold, &host, &server)) != IRC_OK) {
        IRC_MFree(3, &prefix, &nick, &msg);
        return logIntError(ret, "error @ reactNick -> IRCParse_ComplexUser");
    }
    IRCInterface_ChangeNickThread(nickold, msg);
    sprintf(info, "%s es ahora conocido como %s", nickold, msg);
    info[511] = 0;
    IRCInterface_WriteSystemThread(NULL, info);
    IRCInterface_DeleteNickChannelThread (msg, nickold);
    IRCInterface_AddNickChannelThread (msg, nick, userold, userold, host, NONE);

    IRCMsg_Who(&command, prefix, msg, NULL);
    if (send(sockfd, command, strlen(command), 0) <= 0) {
        IRC_MFree(7, &prefix, &nick, &msg, &nickold, &userold, &host, &server);
        return logIntError(-1, "error @ reactMode -> send");
    }
    IRC_MFree(8, &command, &prefix, &nick, &msg, &nickold, &userold, &host, &server);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando MODE. Este comando cambia los modos del canal
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactMode(char* strin) {
    char *prefix, *channeluser, *mode, *user, *nick, *oUser, *host, *server, info[512], *command;
    long ret, iMode;

    prefix = channeluser = mode = user = nick = oUser = host = server = command = NULL;

    if ((ret = IRCParse_Mode(strin, &prefix, &channeluser, &mode, &user)) != IRC_OK)
        return logIntError(ret, "error @ reactMode -> IRCParse_Mode");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &oUser, &host, &server)) != IRC_OK) {
        IRC_MFree(4, &prefix, &channeluser, &mode, &user);
        return logIntError(ret, "error @ reactMode -> IRCParse_ComplexUser");
    }

    //si los modos se refieren a los modos de un canal
    if (user == NULL) {
        iMode = IRCInterface_ModeToIntModeThread(mode);
        if (mode[0] == '-')
            IRCInterface_DeleteModeChannelThread(channeluser, iMode);
        else if (mode[0] == '+')
            IRCInterface_AddModeChannelThread(channeluser, iMode);
        sprintf(info, "%s establece modo %s %s", nick, mode, channeluser);
        info[511] = 0;
        IRCInterface_WriteChannelThread(channeluser, NULL, info);
    } else {
        if (strcmp("+o", mode) == 0)
            IRCInterface_ChangeNickStateChannelThread(channeluser, user, OPERATOR);
        else if (strcmp("-o", mode) == 0)
            IRCInterface_ChangeNickStateChannelThread(channeluser, user, NONE);
        else if (strcmp("+v", mode) == 0)
            IRCInterface_ChangeNickStateChannelThread(channeluser, user, VOICE);
        else if (strcmp("-v", mode) == 0)
            IRCInterface_ChangeNickStateChannelThread(channeluser, user, NONE);
    }
    //Who tras cambiar el mode para actualizar interfaz
    free(prefix);
    prefix = NULL;
    IRCMsg_Who(&command, prefix, channeluser, NULL);
    if (send(sockfd, command, strlen(command), 0) <= 0) {
        IRC_MFree(7, &prefix, &channeluser, &nick, &user, &host, &server, &command);
        return logIntError(-1, "error @ reactMode -> send");
    }
    IRC_MFree(7, &prefix, &channeluser, &nick, &user, &host, &server, &command);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando SERVICE
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactService(char* strin) {
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando JOIN.
 * Si este cliente es quien ha solicitado unirse a un canal, se crea una pestana con este nuevo canal
 * en este caso, para actualizar la lista de usuarios en el canal, se envia al servidor un comando who
 * tambien se envia un comando mode para preguntar por los modos actuales del canal
 * Si no ha sido este cliente quien se unio, se actualiza la lista de usuarios del canal con el nuevo usuario
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactJoin(char *strin) {
    char *prefix, *channel, *key, *msg, *nick, *user, *host, *command, *server, info[512];
    char *myNick, *myUser, *myRealname, *myPassword, *myServer;
    int myPort, mySsl;
    long ret;

    prefix = channel = key = msg = nick = user = host = command = server = myNick = myUser = myRealname = myPassword = myServer = NULL;
    //por si no cabe la linea anterior en pantalla: todos iguales a NULL

    if ((ret = IRCParse_Join(strin, &prefix, &msg, &key, &channel)) != IRC_OK)
        return logIntError(ret, "error @ reactJoin -> IRCParse_Join");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &user, &host, &server)) != IRC_OK) {
        IRC_MFree(4, &prefix, &msg, &key, &channel);
        return logIntError(ret, "error @ reactJoin IRCParse_ComplexUser");
    }
    IRCInterface_GetMyUserInfo(&myNick, &myUser, &myRealname, &myPassword, &myServer, &myPort, &mySsl);
    if (strcmp(myNick, nick) == 0)//quiere decir que yo me uni al canal
    {
        IRCInterface_AddNewChannelThread(channel, 0);
        //README el 5 argumento he puesto server en lugar de host. Esto es probable que sea por un fallo de las librerias. Si falla aqui, revisar
        IRCInterface_AddNickChannelThread(channel, nick, user, "", server, NONE);
        sprintf(info, "te has unido a %s", channel);
        info[511] = 0;
        IRCInterface_WriteChannelThread(channel, NULL, info);
        IRCMsg_Mode(&command, prefix, channel, NULL, NULL);
        if (send(sockfd, command, strlen(command), 0) <= 0) {
            IRC_MFree(14, &prefix, &msg, &key, &channel, &nick, &user, &host, &server, &myNick, &myUser, &myRealname,
                      &myPassword, &myServer, &command);
            return logIntError(-1, "error @ reactJoin -> send");
        }
        IRCInterface_PlaneRegisterInMessageThread(command);
        free(command);
        command = NULL;
        IRCMsg_Who(&command, prefix, channel, NULL);
        if (send(sockfd, command, strlen(command), 0) <= 0) {
            IRC_MFree(14, &prefix, &msg, &key, &channel, &nick, &user, &host, &server, &myNick, &myUser, &myRealname,
                      &myPassword, &myServer, &command);
            return logIntError(-1, "error @ reactJoin -> send");
        }
        IRCInterface_PlaneRegisterInMessageThread(command);
    } else if (IRCInterface_QueryChannelExistThread(channel) == TRUE) {
        //README el 5 argumento he puesto server en lugar de host. Esto es probable que sea por un fallo de las librerias. Si falla aqui, revisar
        IRCInterface_AddNickChannelThread(channel, nick, user, "", server, NONE);
    }
    IRC_MFree(14, &prefix, &msg, &key, &channel, &nick, &user, &host, &server, &myNick, &myUser, &myRealname,
              &myPassword, &myServer, &command);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando PART.
 * Si este cliente ha solicitado salir del canal, la pestaña correspondiente a este canal desaparecerá
 * Si ha sido otro cliente el que ha hecho el part, se le elimina de la lista de usuarios en el canal
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactPart(char* strin) {
    char *prefix, *channel, *nick, *myNick, *msg, *user, *host, *server, info[512];
    long ret;

    prefix = channel = nick = myNick = msg = user = host = server = NULL;

    if ((ret = IRCParse_Part(strin, &prefix, &channel, &msg)) != IRC_OK)
        return logIntError(ret, "error @ reactPart -> IRCParse_Part");
    IRCParse_ComplexUser(prefix, &nick, &user, &host, &server);
    myNick = getMyNickThread();
    IRCInterface_DeleteNickChannelThread(channel, nick);

    sprintf(info, "%s (%s) ha abandonado %s (%s)", nick, prefix, channel, nick);
    info[511] = 0;
    IRCInterface_WriteChannelThread(channel, NULL, info);
    if (strcmp(nick, myNick) == 0)
        IRCInterface_RemoveChannelThread(channel);
    IRC_MFree(3, &prefix, &channel, &nick);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando TOPIC. Cambia el topico del canal
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactTopic(char* strin) {
    char *prefix, *channel, *topic, *nick, *user, *host, *server, info[512];
    long ret;

    prefix = channel = topic = nick = user = host = server = NULL;

    if ((ret = IRCParse_Topic(strin, &prefix, &channel, &topic)) != IRC_OK)
        return logIntError(ret, "error @ reactTopic -> IRCParse_Topic");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &user, &host, &server)) != IRC_OK) {
        IRC_MFree(3, &prefix, &channel, &topic);
        return logIntError(ret, "error @ reactTopic -> IRCParse_ComplexUser");
    }
    IRCInterface_SetTopicThread(topic);
    sprintf(info, "%s ha cambiado el topic a: %s", nick, topic);
    info[511] = 0;
    IRCInterface_WriteChannelThread(channel, NULL, info);
    IRC_MFree(7, &prefix, &channel, &topic, &nick, &user, &host, &server);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando KICK
 * Si el objetivo del kick es este cliente, entonces se elimina la pestana del canal del cual le han expulsado
 * de lo contrario, se elimina al usuario expulsado de la lista de usuarios del canal
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactKick(char* strin) {
    char *prefix, *channel, *victim, *nick, *user, *host, *server, *comment, *myNick, info[512];
    long ret;

    prefix = channel = victim = nick = user = host = server = comment = myNick = NULL;

    if ((ret = IRCParse_Kick(strin, &prefix, &channel, &victim, &comment)) != IRC_OK)
        return logIntError(ret, "error @ reactKick -> IRCParse_Kick");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &user, &host, &server)) != IRC_OK) {
        IRC_MFree(4, &prefix, &channel, &victim, &comment);
        return logIntError(ret, "error @ reactKick -> IRCParse_ComplexUser");
    }
    myNick = getMyNickThread();
    IRCInterface_DeleteNickChannelThread(channel, victim);
    sprintf(info, "%s ha expulsado a %s de %s (%s)", nick, victim, channel, nick);
    info[511] = 0;
    IRCInterface_WriteChannelThread(channel, NULL, info);
    if (strcmp(myNick, victim) == 0)
        IRCInterface_RemoveChannelThread(channel);
    IRC_MFree(8, &prefix, &channel, &victim, &comment, &nick, &user, &host, &server);
    return IRC_OK;
}

/**
 * @brief Esta rutina se encarga de parsear un comando PrivMsg que, segun el "protocolo" que establece nuestro cliente, indica una
 * peticion de envio de fichero. para mas informacion sobre este "protocolo" ver la documentacion de threadSend del fichero G-2301-05-P2-xchat.c
 *
 * @param strin el string que contiene los datos a parsear
 * @param filename un puntero a string en el que se almacenara el nombre del fichero
 * @param hostname un puntero a string en el que se almacenara el hostname de quien envia la peticion
 * @param port un puntero a entero donde se almacenara el puerto con el que establecer la conexion
 * @param length un puntero a entero largo sin signo donde se almacenara la longitud del fichero a enviar
 *
 * @return IRC_OK en caso de ir todo bien. Otro valor de lo contrario
 */
int FSend_Parse(char*strin, char**filename, char**hostname, int*port, unsigned long *length) {
    char fn[255], hn[255];
    char* i = strin;
    char* iniFile;
    hn[0] = 0;
    *port = 0;
    *length = 0;
    if (strin == NULL || strin[0] != '\002' || filename == NULL || hostname == NULL || port == NULL || length == NULL)
        return logIntError(-1, "error @ FSend_Parse -> arguments not valid");
    while (*i != '\001') {
        if (*i == 0)
            return logIntError(-1, "error @ FSend_Parse -> loop while");
        i++;
    }
    i++; //nos colocamos al principio del nombre del fichero
    iniFile = i;
    while (*i != '\001') {
        if (*i == 0)
            return logIntError(-1, "error @ FSend_Parse -> loop while");
        i++;
    }//cuando acabemos el bucle estaremos en el \001 del final del nombre del fichero
    memset(fn, 0, 255);
    memcpy(fn, iniFile, i - iniFile);//con esto deberíamos tener en fn el nombre del fichero (aceptando espacios entre medias)
    sscanf(i, "\001 %s %d %lu", hn, port, length); //ahora leeremos strin PERO desde i
    if (hn[0] == 0 || *port == 0 || *length == 0)
        return logIntError(-1, "error @ FSend_Parse -> sscanf");
    *filename = (char*)malloc(sizeof(char) * (strlen(fn) + 1));
    if (*filename == NULL) {
        *port = -1;
        *length = -1;
        return logIntError(-1, "error @ FSend_Parse -> malloc (1)");
    }
    *hostname = (char*)malloc(sizeof(char) * (strlen(fn) + 1));
    if (*hostname == NULL) {
        free(*filename);
        *filename = NULL;
        *port = -1;
        *length = -1;
        return logIntError(-1, "error @ FSend_Parse -> malloc (2)");
    }
    strcpy(*filename, fn);
    strcpy(*hostname, hn);
    return IRC_OK;
}

/**
 * @brief rutina que se encarga de la recepción de ficheros siguiendo el "protocolo" implementado por este cliente
 *
 * @param args los argumentos que el hilo requiere para realizar la recepción del fichero. son del tipo "struct threadRecvArgs"
 *
 * @return NULL
 */
void* threadRecv(void* args) {
    struct threadRecvArgs *aux;
    char* filename, *hostname, *sender, *data; //sender no se libera por ser memoria estatica en la estructura
    int port, sock;
    unsigned long length, index = 0;
    boolean answer;
    FILE* fp;

    //primero nos detacheamos, para acabar por nuestra cuenta sin que le tenga que importar al hilo principal
    pthread_detach(pthread_self());
    //obtenemos de los argumentos todos los datos necesarios
    aux = (struct threadRecvArgs*) args;
    filename = aux->filename;
    hostname = aux->hostname;
    port = aux->port;
    length = aux->length;
    sender = aux->sender;
    //preguntamos al usuario si quiere recibir el archivo o no
    answer = IRCInterface_ReceiveDialogThread(sender, filename);
    //abrimos un socket TCP
    sock = openSocket_TCP();
    if (sock < 0) { //si da error, no podemos continuar
        IRC_MFree(3, &filename, &hostname, &aux);
        logIntError(-1, "error @ threadRecv -> openSocket_TCP");
        return NULL;
    }
    //nos conectamos al puerto que nos ha especificado el emisor
    if (connectTo(sock, hostname, port) < 0) { //si da error, no podemos continuar
        IRC_MFree(3, &filename, &hostname, &aux);
        close(sock);
        logIntError(-1, "error @ threadRecv -> connectTo");
        return NULL;
    }
    //enviamos la respuesta al emisor mediante esta conexion TCP establecida
    send(sock, &answer, sizeof(answer), 0);
    if (answer == FALSE) { //si la respuesta es no querer recibir el archivo, se acabo
        close(sock);
        IRC_MFree(3, &filename, &hostname, &aux);
        return NULL;
    }//de lo contrario continuamos
    //abrimos un archivo con el nombre especificado y procederemos a escribir en el todos los datos
    fp = fopen(filename, "w");
    if (fp == NULL) { //si nos da error, no podemos continuar
        IRC_MFree(3, &filename, &hostname, &aux);
        close(sock);
        logIntError(-1, "error @ threadRecv -> malloc");
        return NULL;
    }
    data = (char*)malloc(sizeof(char) * FILE_BUFLEN);
    if (data == NULL) { //si da error el malloc, no podemos continuar
        IRC_MFree(3, &filename, &hostname, &aux);
        close(sock);
        fclose(fp);
        logIntError(-1, "error @ threadRecv -> malloc");
        return NULL;
    }
    //recibimos los datos del fichero
    while(length - index >= FILE_BUFLEN){
        if (recv(sock, data, FILE_BUFLEN, 0) <= 0) { //si da error, no podemos continuar
            IRC_MFree(4, &filename, &hostname, &aux, &data);
            close(sock);
            fclose(fp);
            logIntError(-1, "error @ threadRecv -> recv");
            return NULL;
        }
        fwrite(data, sizeof(*data), FILE_BUFLEN, fp);
        index += FILE_BUFLEN;
    }
    if(length > index){
        if (recv(sock, data, length - index, 0) <= 0) { //si da error, no podemos continuar
            IRC_MFree(4, &filename, &hostname, &aux, &data);
            close(sock);
            fclose(fp);
            logIntError(-1, "error @ threadRecv -> recv");
            return NULL;
        }
        fwrite(data, sizeof(*data), length - index, fp);
    }
    //una vez hemos acabado esto, liberamos memoria, cerramos sockets y se acabo
    IRC_MFree(4, &filename, &hostname, &aux, &data);
    fclose(fp);
    close(sock);
    return NULL;
}

/**
 * @brief rutina que se encarga de la recepción de audio siguiendo el "protocolo" implementado por este cliente
 *
 * @param args los argumentos que el hilo requiere para realizar la recepción y reproduccion de audio. son del tipo "struct threadAudioArgs"
 *
 * @return NULL
 */
void *threadAudio(void* args){
    char* sender, *hostname;
    int port;
    struct threadAudioArgs* aux;
    boolean answer;
    int sock;

    if (args == NULL)
        return NULL;
    aux = (struct threadAudioArgs*)args;
    sender = aux->sender;
    hostname = aux->hostname;
    port = aux->port;

    answer = IRCInterface_ReceiveDialogThread(sender, "audio en vivo");
    sock = openSocket_TCP();
    if (sock < 0) { //si da error, no podemos continuar
        IRC_MFree(3, &sender, &hostname, &aux);
        logIntError(-1, "error @ threadAudio -> openSocket_TCP");
        return NULL;
    }
    if (connectTo(sock, hostname, port) < 0) {
        IRC_MFree(3, &sender, &hostname, &aux);
        close(sock);
        logIntError(-1, "error @ threadAudio -> connectTo");
        return NULL;
    }
    if(send(sock, &answer, sizeof(boolean), 0) < 0){
        IRC_MFree(3, &sender, &hostname, &aux);
        close(sock);
        logIntError(-1, "error @ threadAudio -> send");
        return NULL;
    }
    if(answer == FALSE){
        IRC_MFree(3, &sender, &hostname, &aux);
        close(sock);
        logIntError(-1, "error @ threadAudio -> send");
        return NULL;
    }
    close(sock);
    initiateReciever();
    IRC_MFree(3, &sender, &hostname, &aux);
    return NULL;
}


/**
 * @brief rutina que maneja la llegada de un comando PRIVMSG. maneja envio de mensajes tanto a canales como a usuarios
 * si en la interfaz no existe la conversacion en la que se participa, se crea una nueva pestana con esta.
 * tambien maneja un caso particular de PrivMsg que sirve para identificar una peticion de envio de ficheros
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactPrivmsg(char* strin) {
    char *prefix, *msgtarget, *msg, *nick, *user, *host, *server, *filename, *hostname, aux[512];
    int portTCP;//, portUDP;
    long ret;
    unsigned long length;
    pthread_t th;
    struct threadAudioArgs *argsAudio;
    struct threadRecvArgs *argsFile;

    prefix = msgtarget = msg = nick = user = host = server = NULL;

    if ((ret = IRCParse_Privmsg(strin, &prefix, &msgtarget, &msg)) != IRC_OK)
        return logIntError(ret, "error @ reactPrivmsg -> IRCParse_Privmsg");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &user, &host, &server)) != IRC_OK) {
        IRC_MFree(3, &prefix, &msgtarget, &msg);
        return logIntError(ret, "error @ reactPrivmsg -> IRCParse_ComplexUser");
    }
    if (msgtarget[0] != '#') { //si no era un mensaje a un canal => era un mensaje privado => escribimos el mensaje en la conversacion de quien lo envia
        free(msgtarget);
        msgtarget = nick;
    }
    if (IRCInterface_QueryChannelExist (msgtarget) == FALSE) //si el canal en el que queremos escribir no existe, lo creamos
        IRCInterface_AddNewChannelThread(msgtarget, 0);

    //ahora veremos las distintas posibilidades de tipo de mensaje privado:

    if (*msg == '\002') { //si es un mensaje para enviar un fichero
        if (FSend_Parse(msg, &filename, &hostname, &portTCP, &length) != IRC_OK) { //parseamos el mensaje. si da error, no podemos continuar
            IRC_MFree(5, &prefix, &msgtarget, &msg, &filename, &hostname);
            return logIntError(-1, "error @ reactPrivmsg -> FSend_Parse");
        }
        //reservamos memoria para la estructura de argumentos que le vamos a pasar al hilo
        argsFile = (struct threadRecvArgs*)malloc(sizeof(struct threadRecvArgs));
        if (argsFile == NULL) { //si da error, no podemos continuar
            IRC_MFree(5, &prefix, &msgtarget, &msg, &filename, &hostname);
            return logIntError(-1, "error @ reactPrivmsg -> malloc");
        }
        //asignamos cada uno de los campos de esta estructura
        argsFile->hostname = hostname;
        argsFile->port = portTCP;
        argsFile->filename = filename;
        argsFile->length = length;
        strcpy(argsFile->sender, nick);
        //lanzamos el hilo
        pthread_create(&th, NULL, threadRecv, argsFile);
    } else if (*msg == '\001') { //si es un mensaje para enviar audio
        if(sscanf(msg, "\001FAUDIO %s %d", aux, &portTCP) < 2){//parseamos el mensaje. Si da error, no podemos continuar
            IRC_MFree(3, &prefix, &msgtarget, &msg);
            return logIntError(-1, "error @ reactPrivmsg -> sscanf");
        }
        //reservamos memoria para la estructura de argumentos que le vamos a pasar al hilo
        argsAudio = (struct threadAudioArgs*)malloc(sizeof(struct threadAudioArgs));
        if (argsAudio == NULL) { //si da error, no podemos continuar
            IRC_MFree(3, &prefix, &msgtarget, &msg);
            return logIntError(-1, "error @ reactPrivmsg -> malloc(2)");
        }
        //reservamos memoria para el campo sender de la estructura de argumentos
        argsAudio->sender = (char*)malloc(sizeof(char) * (strlen(nick) + 1));
        if (argsAudio->sender == NULL) { //si da error, no podemos continuar
            IRC_MFree(4, &prefix, &msgtarget, &msg, &argsAudio);
            return logIntError(-1, "error @ reactPrivmsg -> malloc(3)");
        }
        //copiamos en esta memoria el nick del string que contiene el nombre de quien enviaba esta peticion
        strcpy(argsAudio->sender, nick);
        //reservamos memoria para el campo hostname de la estructura de argumentos
        argsAudio->hostname = (char*)malloc(sizeof(char) * (strlen(aux) + 1));
        if (argsAudio->hostname == NULL) { //si da error, no podemos continuar
            IRC_MFree(5, &(argsAudio->sender), &prefix, &msgtarget, &msg, &argsAudio);
            return logIntError(-1, "error @ reactPrivmsg -> malloc(4)");
        }
        //copiamos en esta memoria, los datos correspondientes
        strcpy(argsAudio->hostname, aux);
        argsAudio->port = portTCP;
        //lanzamos el hilo
        pthread_create(&th, NULL, threadAudio, argsAudio);
    } else {//si es un mensaje normal
        //escribimos en el canal que haga falta y se acabo
        IRCInterface_WriteChannelThread(msgtarget, nick, msg);
        IRC_MFree(3, &prefix, &msgtarget, &msg);
    }
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando PING
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactPing(char* strin) {
    char* command;
    command = calloc(30, sizeof (char));
    if (command == NULL)
        return logIntError(-1, "error @ reactPing -> calloc");
    snprintf(command, 30, "PONG %s", CLIENTNAME);
    if (send(sockfd, "PONG JohnTitor", strlen("PONG JohnTitor"), 0) <= 0) {
        free(command);
        return logIntError(-1, "Error @ IRCInterface_Connect -> send");
    }
    free(command);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando SETNAME
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactSetName(char* strin) {
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando NAMES
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactNames(char* strin) {
    char *prefix, *nick, *type, *channel, *msg, *myNick, info[512];
    long ret;

    prefix = nick = type = channel = msg = myNick = NULL;

    if ((ret = IRCParse_RplNamReply(strin, &prefix, &nick, &type, &channel, &msg)) != IRC_OK)
        return logIntError(ret, "error @ reactNames -> IRCParse_RplNamReply");
    myNick = getMyNickThread();
    if (strcmp(myNick, nick) == 0) {
        sprintf(info, "usuarios en %s: %s", channel, msg);
        info[511] = 0;
        IRCInterface_WriteChannelThread(channel, NULL, info);
    }
    IRC_MFree(6, &prefix, &nick, &type, &channel, &msg, &myNick);
    return IRC_OK;
}

/**
 * @brief rutina que maneja la llegada de un comando QUIT. Se asume que solo se recibirá un quit de otro cliente
 *
 * @param strin el mensaje que envia el servidor
 *
 * @return IRC_OK
 */
long reactQuit(char* strin) {
    char *prefix, *msg, *myNick, *nick, *user, *host, *server, **channels;
    int num, i;
    long ret;

    prefix = msg = myNick = nick = user = host = server = NULL;
    channels = NULL;
    num = i = 0;

    if ((ret = IRCParse_Quit (strin, &prefix, &msg)) != IRC_OK)
        return logIntError(ret, "error @ reactQuit -> IRCParse_Quit");
    if ((ret = IRCParse_ComplexUser(prefix, &nick, &user, &host, &server)) != IRC_OK) {
        IRC_MFree(2, &prefix, &msg);
        return logIntError(ret, "error @ reactQuit -> IRCParse_ComplexUser");
    }
    myNick = getMyNickThread();
    if (strcmp(nick, myNick) != 0){
        IRCInterface_ListAllChannelsThread(&channels, &num);
        for (i = num - 1; i >= 0; i--) {
            IRCInterface_DeleteNickChannelThread(channels[i], nick);
            free(channels[i]);
            channels[i] = NULL;
        }
    } else {
        IRCInterface_ListAllChannelsThread (&channels, &num);
        for (i = num - 1; i >= 0; i--) {
            IRCInterface_RemoveChannelThread(channels[i]);
            free(channels[i]);
            channels[i] = NULL;
        }
    }
    IRC_MFree(8, &prefix, &msg, &nick, &user, &host, &server, &myNick, &channels);
    return IRC_OK;
}