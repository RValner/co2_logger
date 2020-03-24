#ifndef CO2_LOGGER_STATE_H
#define CO2_LOGGER_STATE_H

/*
 * Define the possible states
 */
namespace Status
{
  enum Status
  {
    INITIALIZE,
    WORKING,
    ERROR
  };
  
  char* toString(Status status)
  {
    if (status == INITIALIZE)    return "INITIALIZE";
    else if (status == WORKING)  return "WORKING";
    else if (status == ERROR)    return "ERROR";
  }
}

/*
 * Declare a helper struct for managing the states
 */
struct State
{ 
  State()
  {
    parent_state_ptr = NULL;
    setToInitialize();
  }
  
  State(State* parent_state_ptr_in)
  {
    parent_state_ptr = parent_state_ptr_in;
    setToInitialize();
  }
  
  void setParentStatePointer(State* parent_state_ptr_in)
  {
    parent_state_ptr = parent_state_ptr_in;
  }
  
  void updateStatus(Status::Status new_status)
  {
    status_previous = status;
    status = new_status;
  }
  
  void setToInitialize()
  {
    updateStatus(Status::INITIALIZE);
    if (parent_state_ptr != NULL)
    {
      parent_state_ptr->updateStatus(Status::INITIALIZE);
    }
  }
  
  void setToWorking()
  {
    updateStatus(Status::WORKING);
    // No parent updates required
  }
  
  void setToError()
  {
    updateStatus(Status::ERROR);
    if (parent_state_ptr != NULL)
    {
      parent_state_ptr->updateStatus(Status::ERROR);
    }
  }
  
  Status::Status status = Status::INITIALIZE;
  Status::Status status_previous;
  State* parent_state_ptr;
};

#endif

