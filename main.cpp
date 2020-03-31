#include <map>
#include <libgen.h>
#include "csv.h"

class Stats {
public:
    void increment(Stats &stats) {
        confirmed_count += stats.confirmed_count;
        death_count     += stats.death_count;
        recovered_count += stats.recovered_count;
        active_count    += stats.active_count;
    }

    int64_t confirmed_count = 0;
    int64_t death_count     = 0;
    int64_t recovered_count = 0;        
    int64_t active_count    = 0;
};
    
class DataLevel;
class DataLevel {
public:
    /* Increments stats for this data level and returns next data level with provided name (if any) */
    DataLevel *addData(const std::string &sub_data_level_name,
                       Stats             &stats) {
        /* Increment stats at this level */
        _stats.increment(stats);

        /* Return sub-level data */
        return &_sub_data_level_map[sub_data_level_name];
    }

    DataLevel *getDataForSubLevel(const std::string &sub_data_level_name) {
        auto it = _sub_data_level_map.find(sub_data_level_name);
        if (it != _sub_data_level_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const std::map<std::string, DataLevel> &getSubDataMap(void) { return _sub_data_level_map; }

    Stats getStats(void) const { return _stats; };

private:
    std::map<std::string, DataLevel> _sub_data_level_map;
    Stats                            _stats;
};

typedef std::map<std::string, DataLevel> WorldTimeSeries; /* date_string, data */

DataLevel *LoadV1(WorldTimeSeries   &world_timeseries,
                  const std::string &filepath)
{
    std::string filename     = basename((char *) filepath.c_str());
    std::string::size_type n = filename.find(".csv"); 
    std::string date_string  = filename.substr(0, n);
    
    DataLevel world;
    
    io::CSVReader<5, io::trim_chars<>, io::double_quote_escape<',','\"'>> in(filepath);
    in.read_header(io::ignore_extra_column, "Province/State", "Country/Region", "Confirmed", "Deaths", "Recovered");
    
    {
        std::string state; 
        std::string country;
        Stats       stats;
        
        while (in.read_row(state, 
                           country, 
                           stats.confirmed_count, 
                           stats.death_count, 
                           stats.recovered_count)) {
            DataLevel *country_level  = world.addData(country, stats);
            DataLevel *state_level    = country_level->addData(state, stats);
            state_level->addData("", stats);
        }
    }

    world_timeseries[date_string] = std::move(world);

    return &world_timeseries[date_string];
}


DataLevel *LoadV2(WorldTimeSeries   &world_timeseries,
                  const std::string &filepath)
{
    std::string filename     = basename((char *) filepath.c_str());
    std::string::size_type n = filename.find(".csv"); 
    std::string date_string  = filename.substr(0, n);
    
    DataLevel world;
    
    io::CSVReader<7, io::trim_chars<>, io::double_quote_escape<',','\"'>> in(filepath);
    in.read_header(io::ignore_extra_column, "Admin2", "Province_State", "Country_Region", "Confirmed", "Deaths", "Recovered", "Active");
    
    {
        std::string locality; 
        std::string state; 
        std::string country;
        Stats stats;
        
        while (in.read_row(locality, 
                           state, 
                           country, 
                           stats.confirmed_count, 
                           stats.death_count, 
                           stats.recovered_count,
                           stats.active_count)) {
            DataLevel *country_level  = world.addData(country, stats);
            DataLevel *state_level    = country_level->addData(state, stats);
            DataLevel *locality_level = state_level->addData(locality, stats);
            locality_level->addData("", stats);
        }
    }
    
    world_timeseries[date_string] = std::move(world);

    return &world_timeseries[date_string];
}

int main(int argc, char **argv)
{
    WorldTimeSeries world_timeseries;
    typedef enum {
        QueryLevel_Country = 0,
        QueryLevel_State,
        QueryLevel_Locality,
        
        QueryLevel_Count,
    } QueryLevel;
    std::string level_query_string [QueryLevel_Count] = { "*", "*", "*" };
    
    if (argc < 3) {
        printf("%s Country.State.Locality DatedFile0.csv DatedFile0.csv ...\n", argv[0]);
        return 1;
    }

    /* Determine location search criteria */
    {
        std::string query_string(argv[1]);
        std::string::size_type n;
        bool                   end = false;
        
        for (int i = 0; ((i < QueryLevel_Count) && (!end)); i ++) {
            n = query_string.find(".");
            if (n == std::string::npos) {
                n   = query_string.size();
                end = true;
            }
            level_query_string[i] = query_string.substr(0, n);
            if (!end) {
                query_string = query_string.substr(n + 1);
            }
        }
    }

    /* Load data */
    for (int i = 2; i < argc; i ++) {
        try {
            LoadV1(world_timeseries, argv[i]);
            //printf("Loaded V1 format: %s\n", argv[i]);
        }
        catch (...) {
            try {
                LoadV2(world_timeseries, argv[i]);
                //printf("Loaded V2 format: %s\n", argv[i]);
            }
            catch (...) {
                //printf("Skipping %s\n", argv[i]);
            }
        }
    }

    std::string csv_output_string;
    std::map<std::string, int> location_name_map;
    
    /* Gather location names */
    for (auto time_it : world_timeseries) {
        DataLevel *cur_level_data = &time_it.second;
        
        for (int i = 0; (cur_level_data && (i < QueryLevel_Count)); i ++) {
            if (level_query_string[i] == "*") {
                const std::map<std::string, DataLevel> cur_level_map = cur_level_data->getSubDataMap();

                for (auto it : cur_level_map) {
                    if (it.first.size()) {
                        location_name_map[it.first] ++;
                    }
                }
                
                break;
            } else {
                cur_level_data = cur_level_data->getDataForSubLevel(level_query_string[i]);
            }
        }
    }

    /* Dump column headers */
    csv_output_string += "Date,";
    for (auto it : location_name_map) {
        csv_output_string += "\"" + it.first + "\",";
    }
    csv_output_string += "\n";

    //std::map<std::string, int64_t> location_prev;

    /* Dump data */
    for (auto time_it : world_timeseries) {
        const std::string &date_string    = time_it.first;
        DataLevel         *cur_level_data = &time_it.second;
        
        for (int i = 0; (cur_level_data && (i < QueryLevel_Count)); i ++) {
            if (level_query_string[i] == "*") {
                const std::map<std::string, DataLevel> cur_level_map = cur_level_data->getSubDataMap();
            
                csv_output_string += date_string + ",";
                for (auto location_it : location_name_map) {
                    auto data_it = cur_level_map.find(location_it.first);

                    if (data_it != cur_level_map.end()) {
                        csv_output_string += std::to_string(data_it->second.getStats().confirmed_count) + ",";
                    } else {
                        csv_output_string += "0,";
                    }
                }
                csv_output_string += "\n";
                
                break;
            } else {
                cur_level_data = cur_level_data->getDataForSubLevel(level_query_string[i]);
            }
        }
    }

    printf("%s", csv_output_string.c_str());

    return 0;
}
