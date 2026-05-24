#ifndef PACKETCODEC_H
#define PACKETCODEC_H

#include <QByteArray>
#include "protocol.h"

class PacketCodec
{
public:
    void append(const QByteArray &data);
    PDU *takePacket();
    QByteArray takeBufferedData();
    void clear();
    int bufferedSize() const;

private:
    static bool isValidPduLength(unit pduLength);

    QByteArray m_buffer;
};

#endif // PACKETCODEC_H

