#pragma once
#include <stdint.h>
#include <cstddef>

class DataManager
{
    static constexpr uint8_t MEAN_COUNT     = 10u;
    static constexpr uint8_t MAX_DATA_COUNT = 100u;
    static constexpr uint8_t REGIONS_COUNT  = 126u;

public:
    DataManager();

    bool
    receive_idx_data(char mqtt_data, size_t idx);

    bool
    set_idx_data_terminator(size_t idx);

    bool
    receive_data(char mqtt_data, size_t idx);

    bool
    set_data_terminator(size_t idx);

    void
    parse_data();

    char const* const
    get_message() const;

    size_t
    get_message_len() const;

private:
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

    char m_idx_data[MAX_DATA_COUNT / 10u];
    char m_received_data[MAX_DATA_COUNT];
    Raw_Data m_raw_data[REGIONS_COUNT];
    Sum_Data m_sum_data[REGIONS_COUNT];
    uint8_t m_data_count[REGIONS_COUNT];
    char m_message[MAX_DATA_COUNT];
    size_t m_message_len;
};