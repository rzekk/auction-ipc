#ifndef PROTOCOL_H
#define PROTOCOL_H

struct BidMessage {
    int client_id;
    int amount;
};

#define PORT 8080

#endif