/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <cassert>
#include <algorithm>
#include <string>
#include <uavcan/internal/transport/can_io.hpp>

namespace uavcan
{

enum { MAX_TRANSFER_PAYLOAD_LEN = 439 }; ///< According to the standard

enum TransferType
{
    TRANSFER_TYPE_SERVICE_RESPONSE  = 0,
    TRANSFER_TYPE_SERVICE_REQUEST   = 1,
    TRANSFER_TYPE_MESSAGE_BROADCAST = 2,
    TRANSFER_TYPE_MESSAGE_UNICAST   = 3,
    NUM_TRANSFER_TYPES = 4
};


class NodeID
{
    enum
    {
        VALUE_BROADCAST = 0,
        VALUE_INVALID = 0xFF
    };
    uint8_t value_;

public:
    enum { BITLEN = 7 };
    enum { MAX = (1 << BITLEN) - 1 };

    static const NodeID BROADCAST;

    NodeID() : value_(VALUE_INVALID) { }

    NodeID(uint8_t value)
    : value_(value)
    {
        assert(isValid());
    }

    uint8_t get() const { return value_; }

    bool isValid() const     { return value_ <= MAX; }
    bool isBroadcast() const { return value_ == VALUE_BROADCAST; }
    bool isUnicast() const   { return (value_ <= MAX) && (value_ != VALUE_BROADCAST); }

    bool operator!=(NodeID rhs) const { return !operator==(rhs); }
    bool operator==(NodeID rhs) const { return value_ == rhs.value_; }
};


class TransferID
{
    uint8_t value_;

public:
    enum { BITLEN = 3 };
    enum { MAX = (1 << BITLEN) - 1 };

    TransferID()
    : value_(0)
    { }

    TransferID(uint8_t value)    // implicit
    : value_(value)
    {
        value_ &= MAX;
        assert(value == value_);
    }

    bool operator!=(TransferID rhs) const { return !operator==(rhs); }
    bool operator==(TransferID rhs) const { return get() == rhs.get(); }

    void increment()
    {
        value_ = (value_ + 1) & MAX;
    }

    uint8_t get() const
    {
        assert(value_ <= MAX);
        return value_;
    }

    /**
     * Amount of increment() calls to reach rhs value.
     */
    int forwardDistance(TransferID rhs) const;
};


class Frame
{
    uint8_t payload_[sizeof(CanFrame::data)];
    TransferType transfer_type_;
    uint_fast16_t data_type_id_;
    uint_fast8_t payload_len_;
    NodeID src_node_id_;
    NodeID dst_node_id_;
    uint_fast8_t frame_index_;
    TransferID transfer_id_;
    bool last_frame_;

public:
    enum { DATA_TYPE_ID_MAX = 1023 };
    enum { INDEX_MAX = 62 };        // 63 (or 0b111111) is reserved

    Frame()
    : transfer_type_(TransferType(NUM_TRANSFER_TYPES))  // That is invalid value
    , data_type_id_(0)
    , payload_len_(0)
    , frame_index_(0)
    , transfer_id_(0)
    , last_frame_(false)
    { }

    Frame(uint_fast16_t data_type_id, TransferType transfer_type, NodeID src_node_id, NodeID dst_node_id,
          uint_fast8_t frame_index, TransferID transfer_id, bool last_frame = false)
    : transfer_type_(transfer_type)
    , data_type_id_(data_type_id)
    , payload_len_(0)
    , src_node_id_(src_node_id)
    , dst_node_id_(dst_node_id)
    , frame_index_(frame_index)
    , transfer_id_(transfer_id)
    , last_frame_(last_frame)
    {
        assert((transfer_type == TRANSFER_TYPE_MESSAGE_BROADCAST) == dst_node_id.isBroadcast());
        assert(data_type_id <= DATA_TYPE_ID_MAX);
        assert(src_node_id != dst_node_id);
        assert(frame_index <= INDEX_MAX);
    }

    int getMaxPayloadLen() const;
    int setPayload(const uint8_t* data, int len);

    int getPayloadLen() const { return payload_len_; }
    const uint8_t* getPayloadPtr() const { return payload_; }

    TransferType getTransferType() const { return transfer_type_; }
    uint_fast16_t getDataTypeID()  const { return data_type_id_; }
    NodeID getSrcNodeID()          const { return src_node_id_; }
    NodeID getDstNodeID()          const { return dst_node_id_; }
    TransferID getTransferID()     const { return transfer_id_; }
    uint_fast8_t getIndex()        const { return frame_index_; }
    bool isLast()                  const { return last_frame_; }

    void makeLast() { last_frame_ = true; }
    void setIndex(uint_fast8_t index) { frame_index_ = index; }

    bool isFirst() const { return frame_index_ == 0; }

    bool parse(const CanFrame& can_frame);
    bool compile(CanFrame& can_frame) const;

    bool isValid() const;

    bool operator!=(const Frame& rhs) const { return !operator==(rhs); }
    bool operator==(const Frame& rhs) const;

    std::string toString() const;
};


class RxFrame : public Frame
{
    uint64_t ts_monotonic_;
    uint64_t ts_utc_;
    uint8_t iface_index_;

public:
    RxFrame()
    : ts_monotonic_(0)
    , ts_utc_(0)
    , iface_index_(0)
    { }

    RxFrame(const Frame& frame, uint64_t ts_monotonic, uint64_t ts_utc, uint8_t iface_index)
    : ts_monotonic_(ts_monotonic)
    , ts_utc_(ts_utc)
    , iface_index_(iface_index)
    {
        *static_cast<Frame*>(this) = frame;
    }

    bool parse(const CanRxFrame& can_frame)
    {
        if (!Frame::parse(can_frame))
            return false;
        ts_monotonic_ = can_frame.ts_monotonic;
        ts_utc_ = can_frame.ts_utc;
        iface_index_ = can_frame.iface_index;
        return true;
    }

    uint64_t getMonotonicTimestamp() const { return ts_monotonic_; }
    uint64_t getUtcTimestamp()       const { return ts_utc_; }

    uint8_t getIfaceIndex() const { return iface_index_; }

    std::string toString() const;
};

}