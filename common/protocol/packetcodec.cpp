#include "packetcodec.h"

#include <cstring>

static const unit MAX_PDU_LENGTH = 64 * 1024 * 1024;

void PacketCodec::append(const QByteArray &data)
{
    if (!data.isEmpty()) {
        m_buffer.append(data);
    }
}

PDU *PacketCodec::takePacket()
{
    if (m_buffer.size() < static_cast<int>(sizeof(unit))) {
        return nullptr;
    }

    unit pduLength = 0;
    std::memcpy(&pduLength, m_buffer.constData(), sizeof(unit));

    if (!isValidPduLength(pduLength)) {
        clear();
        return nullptr;
    }

    if (m_buffer.size() < static_cast<int>(pduLength)) {
        return nullptr;
    }

    unit msgLength = pduLength - sizeof(PDU);
    PDU *pdu = mkPDU(msgLength);
    std::memcpy(pdu, m_buffer.constData(), pduLength);
    m_buffer.remove(0, static_cast<int>(pduLength));
    return pdu;
}

QByteArray PacketCodec::takeBufferedData()
{
    QByteArray data = m_buffer;
    m_buffer.clear();
    return data;
}

void PacketCodec::clear()
{
    m_buffer.clear();
}

int PacketCodec::bufferedSize() const
{
    return m_buffer.size();
}

bool PacketCodec::isValidPduLength(unit pduLength)
{
    return pduLength >= sizeof(PDU) && pduLength <= MAX_PDU_LENGTH;
}

