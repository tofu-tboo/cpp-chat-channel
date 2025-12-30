#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

class Channel {
    private:
        ch_id_t channel_id;
    public:
        Channel();
        ~Channel();

};

#endif