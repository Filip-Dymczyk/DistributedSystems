#pragma once
#include <stdint.h>
#include <cstddef>

class DataManager
{
    static constexpr uint8_t MAX_DATA_COUNT = 100u;

public:
    DataManager();

    void
    set_data_indicator(int mqtt_data);

    char
    get_data_indicator() const;

    bool
    receive_data(int mqtt_data, size_t idx);

    bool
    set_data_terminator(size_t idx);

    void
    parse_data();

    char const* const
    get_message() const;

    size_t
    get_message_len() const;

private:
    enum class IDX : uint8_t
    {
        TEMPERATURE = 0u,
        INSULATION  = 1u,
        WIND        = 2u
    };

    struct Raw_Data
    {
        float temperature {};
        float insulation {};
        float wind {};
    };

    struct Sum_Data
    {
        float temperature {};
        float insulation {};
        float wind {};
    };

    char m_data_indicator;
    uint8_t m_data_count[3];
    char m_received_data[MAX_DATA_COUNT];
    Raw_Data m_raw_data;
    Sum_Data m_sum_data;
    char m_message[MAX_DATA_COUNT];
    size_t m_message_len;

    size_t
    idx_to_int(IDX idx);
};