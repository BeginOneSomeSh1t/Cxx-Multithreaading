#include "Public/popl.hpp"
#include "Public/Preassigned.h"
#include "Public/Queued.h"
#include "Public/Task.h"


int main(int argc, char** argv)
{
    using namespace popl;

    // define and parle cli options
    OptionParser op("Allowed options");
    auto stacked = op.add<Switch>("", "stacked", "Generate a stacked Dataset");
    auto even = op.add<Switch>("", "even", "Generate an even Dataset");
    auto queued = op.add<Switch>("", "queued", "Use queued approach");

    op.parse(argc, argv);
    
    // Determine the data type form the command args
    DatasetType data_type;
    if(stacked->is_set())
    {
        data_type = DatasetType::stacked;
    }
    else if(even->is_set())
    {
        data_type = DatasetType::evenly;
    }
    else
    {
        data_type = DatasetType::random;
    }

    Dataset data = generate_data_sets_by_type(data_type);
    
    // Run experiment
    if(queued->is_set())
    {
        LOG_ALWAYS(LogTemp, Info, "Start queued experiment");
        return que::do_Experiment(std::move(data));
    }
    else
    {
        LOG_ALWAYS(LogTemp, Info, "Start random experiment");
        return pre::do_experiment(std::move(data));
    }
    
    
}