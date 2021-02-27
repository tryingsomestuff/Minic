#include "distributed.h"

#ifdef WITH_MPI

#include "dynamicConfig.hpp"
#include "logging.hpp"

namespace Distributed{
   int worldSize;
   int rank;
   std::string name;

   void init(){
      MPI_Init(NULL, NULL);
      MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      char processor_name[MPI_MAX_PROCESSOR_NAME];
      int name_len;
      MPI_Get_processor_name(processor_name, &name_len);
      name = processor_name;

      if ( !isMainProcess() ) DynamicConfig::silent = true;
   }

   void finalize(){
      MPI_Finalize();
   }

   bool isMainProcess(){
       return rank == 0;
   }

   // barrier
   void sync(){
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "syncing";
       MPI_Barrier(MPI_COMM_WORLD);  
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "...done";
   }

}

#endif