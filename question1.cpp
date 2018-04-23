// ECE 4550 Project 2
// Generates a schedule for RM and EDF. Reads from an input file: # of tasks,
// 		simulation time, task ID, task execution time, task period (with implicit 
// 		deadline.
// To an output file, your program will show the corresponding RM and EDF schedules, 
//		clearly indicating when a job is being preempted and when a job misses its 
//		deadline. Jobs that miss their deadlines should be dropped at their respective
//		deadlines.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::stoi;
using std::ifstream;
using std::ofstream;

struct Task{
	int id;
	double execution_time;
	double period;
	double deadline;
	double left_to_execute;
	double time_to_deadline;
	bool completed;
	int deadlines_missed;
	vector<string> outputs;
	int priority;
	int current_priority;
	int priority_inherited_from;
	bool using_resource;
	int resource_used;
	double time_before_critical_section_start;
	double critical_section_left_to_execute;
	bool done_critical;
	bool waiting_for_critical;
};

struct Resource {
	int id;
	int number_tasks;
	vector<int> tasks_using;
	int priority_ceiling;
	bool in_use;
};

bool compareByID(const Task &a, const Task &b)
{
	return a.id < b.id;
}

bool compareByPeriod(const Task &a, const Task &b)
{
	return a.period > b.period;
}

int main(int argc, char *argv[]) {
	////////////////////////////
	// Parse first input file //
	////////////////////////////
	ifstream in;
	in.open(argv[1]);
	if (!in.is_open()) {
		cout << "Error: Could not open file\n" << endl;
		return false;
	}
	string line;
	string junk;
	struct Task temp = { 0,0,0,0,0,0,false,0,{},0,0,0,false,-1,0,0,false,false };
	vector<Task> tasks;
	double simulation_time;
	// Stores the number of tasks
	getline(in, line, '\n');
	int number_of_tasks = stoi(line);
	// Stores all the tasks in a vector
	for(int i = 0; i < number_of_tasks; i++) {
		getline(in, line, '\n');
		std::istringstream iss(line);
		iss >> temp.id >> temp.execution_time >> temp.period >> temp.deadline >> junk;
		temp.left_to_execute = temp.execution_time;
		temp.time_to_deadline = temp.deadline;
		temp.time_before_critical_section_start = temp.execution_time/4;
		temp.critical_section_left_to_execute = temp.execution_time/2;
		tasks.push_back(temp);
	}
	// Stores the simulation time
	in >> junk >> simulation_time;
	std::sort(tasks.begin(), tasks.end(), compareByID);
	in.close();
	in.clear();

	/////////////////////////////
	// Parse second input file //
	/////////////////////////////
	in.open(argv[2]);
	if (!in.is_open()) {
		cout << "Error: Could not open file\n" << endl;
		return false;
	}
	struct Resource res_temp = { -1,0,{},0,false };
	vector<Resource> resources;
	// Stores the number of resources
	getline(in, line, '\n');
	getline(in, line, '\n');
	int number_of_resources = stoi(line);
	// Stores all the tasks in a vector
	for (int i = 0; i < number_of_resources; i++) {
		getline(in, line, '\n');
		std::istringstream iss(line);
		iss >> res_temp.id >> res_temp.number_tasks;
		int num = -1;
		while (iss >> num) {
			res_temp.tasks_using.push_back(num);
			for (unsigned int i = 0; i < tasks.size(); i++) {
				if (tasks.at(i).id == num)
					tasks.at(i).resource_used = res_temp.id;
			}
		}
		resources.push_back(res_temp);
		res_temp.tasks_using.clear();
	}
	in.close();
	in.clear();

	///////////////////////
	// Assign Priorities //
	///////////////////////
	// Sort all tasks with longest period first. Iterate through all tasks and assign
	// longest period lowest priority (1) to shortest period highest priority.
	std::sort(tasks.begin(), tasks.end(), compareByPeriod);
	for (unsigned int i = 1; i <= tasks.size(); i++) {
		tasks.at(i - 1).priority = i;
		tasks.at(i - 1).current_priority = i;
		tasks.at(i - 1).priority_inherited_from = tasks.at(i - 1).id;
	}
	std::sort(tasks.begin(), tasks.end(), compareByID);
	// For each resource, look at each task that uses that resource. Whatever the 
	// highest priority among those tasks is, the resource gets that priority as its
	// priority ceiling
	int highest_priority = 0;
	for (unsigned int i = 0; i < resources.size(); i++) {
		for (int j = 0; j < resources.at(i).number_tasks; j++) {
			for (unsigned int k = 0; k < tasks.size(); k++) {
				if (resources.at(i).tasks_using.at(j) == tasks.at(k).id)
					if(tasks.at(k).priority > highest_priority)
						highest_priority = tasks.at(k).priority;
			}
		}
		resources.at(i).priority_ceiling = highest_priority;
		highest_priority = 0;
	}

	/////////////////////////////
	// Begin Scheduling RM+PCP //
	/////////////////////////////
	ofstream out;
	out.open("output1.txt");
	bool task_occuring = false;
	bool task_reset = false;
	bool resource_change = false;
	int total_deadlines_missed = 0;
	int last_task_id = -1;
	struct Task shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false,false };
	out << "***RM+PCP SCHEDULING***" << endl;
	for (double current_time = 0; current_time <= simulation_time; current_time+=.25) {
		// Check if any task not complete will miss deadline
		if(current_time != 0.0) {
			for (unsigned int i = 0; i < tasks.size(); i++) {
				// (D = T) Current time has reached deadline and not done executing
				if ((std::fmod(current_time, (tasks.at(i).deadline + ((int)current_time / (int)tasks.at(i).period) * tasks.at(i).period)) == 0 || 
					 std::fmod(current_time, (tasks.at(i).deadline + (((int)current_time / (int)tasks.at(i).period)-1) * tasks.at(i).period)) == 0)
					 && tasks.at(i).left_to_execute != 0) {
					out << "TASK" << tasks.at(i).id << " MISSED DEADLINE" << endl;
					total_deadlines_missed++;
					tasks.at(i).left_to_execute = tasks.at(i).execution_time;
					tasks.at(i).time_before_critical_section_start = tasks.at(i).execution_time / 4;
					tasks.at(i).critical_section_left_to_execute = tasks.at(i).execution_time / 2;
					tasks.at(i).done_critical = false;
					tasks.at(i).deadlines_missed++;
					tasks.at(i).completed = true;
					task_reset = true;
					task_occuring = false;
					tasks.at(i).current_priority = tasks.at(i).priority;
					tasks.at(i).priority_inherited_from = tasks.at(i).id;
					tasks.at(i).waiting_for_critical = false;
					shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false,false };
				}
			}
		}
		// Check if any task's periods have restarted.
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (std::fmod(current_time, tasks.at(i).period) == 0) {
				tasks.at(i).completed = false;
				tasks.at(i).left_to_execute = tasks.at(i).execution_time;
				tasks.at(i).time_before_critical_section_start = tasks.at(i).execution_time / 4;
				tasks.at(i).critical_section_left_to_execute = tasks.at(i).execution_time / 2;
				tasks.at(i).done_critical = false;
				task_reset = true;
				tasks.at(i).waiting_for_critical = false;
			}
		}
		// Check if any task is done using a resource, reset priority
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).critical_section_left_to_execute == 0 && !tasks.at(i).done_critical) {
				tasks.at(i).using_resource = false;
				tasks.at(i).done_critical = true;
				resource_change = true;
				tasks.at(i).current_priority = tasks.at(i).priority;
				tasks.at(i).priority_inherited_from = tasks.at(i).id;
				for (unsigned int j = 0; j < resources.size(); j++) {
					if (resources.at(j).id == tasks.at(i).resource_used)
						resources.at(j).in_use = false;
				}
				tasks.at(i).waiting_for_critical = false;
				for (unsigned int j = 0; j < resources.size(); j++) {
					if (tasks.at(i).resource_used == resources.at(j).id) {
						for (unsigned int k = 0; k < tasks.size(); k++) {
							if (tasks.at(k).resource_used == resources.at(j).id) {
								tasks.at(k).waiting_for_critical = false;
							}
						}
					}
				}
			}
		}
		// Check if any task is ready to use a resource
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).time_before_critical_section_start == 0 && !tasks.at(i).done_critical && !tasks.at(i).using_resource && !tasks.at(i).waiting_for_critical) {
				for (unsigned int j = 0; j < resources.size(); j++) {
					for (int k = 0; k < resources.at(j).number_tasks; k++) {
						if (resources.at(j).tasks_using.at(k) == tasks.at(i).id) {
							std::stringstream stream;
							if (!resources.at(j).in_use) {
								tasks.at(i).waiting_for_critical = false;
								tasks.at(i).using_resource = true;
								resources.at(j).in_use = true;
								resource_change = true;
								stream << std::fixed << std::setprecision(2) << current_time << "," << tasks.at(i).current_priority <<
									"," << tasks.at(i).priority_inherited_from << "\n";
								tasks.at(i).outputs.push_back(stream.str());
							}
							else {
								// Raise the task's priority that is using the resource
								// Set the task inheritance value
								tasks.at(i).waiting_for_critical = true;
								for (unsigned int l = 0; l < tasks.size(); l++) {
									if (tasks.at(l).resource_used == resources.at(j).id && tasks.at(l).using_resource) {
										tasks.at(l).current_priority = resources.at(j).priority_ceiling;
										resource_change = true;
										for (unsigned int m = 0; m < tasks.size(); m++) {
											if (tasks.at(m).priority == resources.at(j).priority_ceiling)
												tasks.at(l).priority_inherited_from = tasks.at(m).id;
										}
										stream << std::fixed << std::setprecision(2) << current_time << "," << tasks.at(l).current_priority <<
											"," << tasks.at(l).priority_inherited_from << "\n";
										tasks.at(l).outputs.push_back(stream.str());
									}
								}
							}
						}
					}
				}
			}
		}
		// If a task was reset, it may have higher priority than what was already running
		// or nothing was running so see if anything needs run, or a resource is now being
		// used and/or was released
		if (task_reset || !task_occuring || resource_change) {
			// Check for the highest priority task of those not completed
			for (unsigned int i = 0; i < tasks.size(); i++) {
				if (tasks.at(i).current_priority >= shortest.current_priority && !tasks.at(i).completed && tasks.at(i).deadlines_missed < 1) {
					shortest = tasks.at(i);
				}
			}
			// There is a task that hasn't been completed, so start it
			if (shortest.period != 32767)
				task_occuring = true;
		}
		// See if task is running and not complete
		if (task_occuring && shortest.left_to_execute != 0) {
			// See if current task isn't the same as last task ran and a task was reset
			// meaning potential preemption occurred.
			if (last_task_id >= 0 && task_reset && !tasks.at(last_task_id).completed && last_task_id != shortest.id) {
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << " Preempted Task" << last_task_id << endl;
			}
			else if (shortest.using_resource) {
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << " Using Resource" << shortest.resource_used << endl;
			}
			else
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << endl;
			last_task_id = shortest.id;
		}
		// All tasks currently completed and free time
		else if (!task_occuring) {
			out << current_time << ":" << endl;
		}
		// Decrement the time the current task still needs to run
		shortest.left_to_execute-=.25;
		if (!shortest.using_resource && !shortest.done_critical)
			shortest.time_before_critical_section_start -= .25;
		else if (shortest.using_resource)
			shortest.critical_section_left_to_execute -= .25;
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).id == shortest.id) {
				tasks.at(i).left_to_execute = shortest.left_to_execute;
				tasks.at(i).time_before_critical_section_start = shortest.time_before_critical_section_start;
				tasks.at(i).critical_section_left_to_execute = shortest.critical_section_left_to_execute;
			}
		}
		// See if task that ran on this runthrough is completed
		if (task_occuring && shortest.left_to_execute == 0) {
			task_occuring = false;
			for (unsigned int i = 0; i < tasks.size(); i++) {
				if (tasks.at(i).id == shortest.id) {
					tasks.at(i).completed = true;
				}
			}
			shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false };
		}
		task_reset = false;
		resource_change = false;
	}
	out.close();
	///////////////////////////
	// Print out PCP Summary //
	///////////////////////////
	ofstream outputs;
	for (unsigned int i = 0; i < tasks.size(); i++) {
		string filename = "PCP/output_" + std::to_string(i) + ".txt";
		outputs.open(filename);
		outputs << "lock_instant,curr_prio,prio_inherit\n";
		for (unsigned int j = 0; j < tasks.at(i).outputs.size(); j++) {
			outputs << tasks.at(i).outputs.at(j);
		}
		outputs.close();
	}
	outputs.open("PCP/deadline_misses.txt");
	for (unsigned int i = 0; i < tasks.size(); i++) {
		outputs << std::to_string(tasks.at(i).id) << " " << tasks.at(i).deadlines_missed << "\n";
	}
	outputs.close();

	//////////////////////////
	// Begin Scheduling EDF //
	//////////////////////////
		
		/* for ICPP
		// Check if any resources are now locked.
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).time_before_critical_section_start == 0) {
				tasks.at(i).using_resource = true;
				resource_change = true;
				for (unsigned int j = 0; j < resources.size(); j++) {
					for (unsigned int k = 0; k < resources.at(j).number_tasks; k++) {
						if (resources.at(j).tasks_using.at(k) == tasks.at(i).id)
							tasks.at(i).current_priority = resources.at(j).priority_ceiling;
					}
				}
			}
		}
		*/
	out.open("output2.txt");
	// Reset preemption and deadlines missed count
	for (unsigned int i = 0; i < tasks.size(); i++) {
		tasks.at(i).deadlines_missed = 0;
	}
	task_occuring = false;
	task_reset = false;
	resource_change = false;
	total_deadlines_missed = 0;
	last_task_id = -1;
	shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false,false };
	out << "***RM+ICCP SCHEDULING***" << endl;
	for (double current_time = 0; current_time <= simulation_time; current_time += .25) {
		// Check if any task not complete will miss deadline
		if (current_time != 0.0) {
			for (unsigned int i = 0; i < tasks.size(); i++) {
				// (D = T) Current time has reached deadline and not done executing
				if ((std::fmod(current_time, (tasks.at(i).deadline + ((int)current_time / (int)tasks.at(i).period) * tasks.at(i).period)) == 0 ||
					std::fmod(current_time, (tasks.at(i).deadline + (((int)current_time / (int)tasks.at(i).period) - 1) * tasks.at(i).period)) == 0)
					&& tasks.at(i).left_to_execute != 0) {
					out << "TASK" << tasks.at(i).id << " MISSED DEADLINE" << endl;
					total_deadlines_missed++;
					tasks.at(i).left_to_execute = tasks.at(i).execution_time;
					tasks.at(i).time_before_critical_section_start = tasks.at(i).execution_time / 4;
					tasks.at(i).critical_section_left_to_execute = tasks.at(i).execution_time / 2;
					tasks.at(i).done_critical = false;
					tasks.at(i).deadlines_missed++;
					tasks.at(i).completed = true;
					task_reset = true;
					task_occuring = false;
					tasks.at(i).current_priority = tasks.at(i).priority;
					tasks.at(i).priority_inherited_from = tasks.at(i).id;
					tasks.at(i).waiting_for_critical = false;
					shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false,false };
				}
			}
		}
		// Check if any task's periods have restarted.
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (std::fmod(current_time, tasks.at(i).period) == 0) {
				tasks.at(i).completed = false;
				tasks.at(i).left_to_execute = tasks.at(i).execution_time;
				tasks.at(i).time_before_critical_section_start = tasks.at(i).execution_time / 4;
				tasks.at(i).critical_section_left_to_execute = tasks.at(i).execution_time / 2;
				tasks.at(i).done_critical = false;
				task_reset = true;
				tasks.at(i).waiting_for_critical = false;
			}
		}
		// Check if any task is done using a resource, reset priority
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).critical_section_left_to_execute == 0 && !tasks.at(i).done_critical) {
				tasks.at(i).using_resource = false;
				tasks.at(i).done_critical = true;
				resource_change = true;
				tasks.at(i).current_priority = tasks.at(i).priority;
				tasks.at(i).priority_inherited_from = tasks.at(i).id;
				for (unsigned int j = 0; j < resources.size(); j++) {
					if (resources.at(j).id == tasks.at(i).resource_used)
						resources.at(j).in_use = false;
				}
				tasks.at(i).waiting_for_critical = false;
				for (unsigned int j = 0; j < resources.size(); j++) {
					if (tasks.at(i).resource_used == resources.at(j).id) {
						for (unsigned int k = 0; k < tasks.size(); k++) {
							if (tasks.at(k).resource_used == resources.at(j).id) {
								tasks.at(k).waiting_for_critical = false;
							}
						}
					}
				}
			}
		}
		// Check if any task is ready to use a resource
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).time_before_critical_section_start == 0 && !tasks.at(i).done_critical && !tasks.at(i).using_resource && !tasks.at(i).waiting_for_critical) {
				for (unsigned int j = 0; j < resources.size(); j++) {
					for (int k = 0; k < resources.at(j).number_tasks; k++) {
						if (resources.at(j).tasks_using.at(k) == tasks.at(i).id) {
							std::stringstream stream;
							if (!resources.at(j).in_use) {
								tasks.at(i).current_priority = resources.at(j).priority_ceiling;
								for (unsigned int m = 0; m < tasks.size(); m++) {
									if (tasks.at(m).priority == resources.at(j).priority_ceiling)
										tasks.at(i).priority_inherited_from = tasks.at(m).id;
								}
								tasks.at(i).waiting_for_critical = false;
								tasks.at(i).using_resource = true;
								resources.at(j).in_use = true;
								resource_change = true;
								stream << std::fixed << std::setprecision(2) << current_time << "," << tasks.at(i).current_priority <<
									"," << tasks.at(i).priority_inherited_from << "\n";
								tasks.at(i).outputs.push_back(stream.str());
							}
							else {
								// Raise the task's priority that is using the resource
								// Set the task inheritance value
								tasks.at(i).waiting_for_critical = true;
							}
						}
					}
				}
			}
		}
		// If a task was reset, it may have higher priority than what was already running
		// or nothing was running so see if anything needs run, or a resource is now being
		// used and/or was released
		if (task_reset || !task_occuring || resource_change) {
			// Check for the highest priority task of those not completed
			for (unsigned int i = 0; i < tasks.size(); i++) {
				if (tasks.at(i).current_priority >= shortest.current_priority && !tasks.at(i).completed && tasks.at(i).deadlines_missed < 1) {
					shortest = tasks.at(i);
				}
			}
			// There is a task that hasn't been completed, so start it
			if (shortest.period != 32767)
				task_occuring = true;
		}
		// See if task is running and not complete
		if (task_occuring && shortest.left_to_execute != 0) {
			// See if current task isn't the same as last task ran and a task was reset
			// meaning potential preemption occurred.
			if (last_task_id >= 0 && task_reset && !tasks.at(last_task_id).completed && last_task_id != shortest.id) {
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << " Preempted Task" << last_task_id << endl;
			}
			else if (shortest.using_resource) {
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << " Using Resource" << shortest.resource_used << endl;
			}
			else
				out << std::setprecision(2) << std::fixed << current_time << ": Task" << shortest.id << endl;
			last_task_id = shortest.id;
		}
		// All tasks currently completed and free time
		else if (!task_occuring) {
			out << current_time << ":" << endl;
		}
		// Decrement the time the current task still needs to run
		shortest.left_to_execute -= .25;
		if (!shortest.using_resource && !shortest.done_critical)
			shortest.time_before_critical_section_start -= .25;
		else if (shortest.using_resource)
			shortest.critical_section_left_to_execute -= .25;
		for (unsigned int i = 0; i < tasks.size(); i++) {
			if (tasks.at(i).id == shortest.id) {
				tasks.at(i).left_to_execute = shortest.left_to_execute;
				tasks.at(i).time_before_critical_section_start = shortest.time_before_critical_section_start;
				tasks.at(i).critical_section_left_to_execute = shortest.critical_section_left_to_execute;
			}
		}
		// See if task that ran on this runthrough is completed
		if (task_occuring && shortest.left_to_execute == 0) {
			task_occuring = false;
			for (unsigned int i = 0; i < tasks.size(); i++) {
				if (tasks.at(i).id == shortest.id) {
					tasks.at(i).completed = true;
				}
			}
			shortest = { -1,-1,32767,-1,-1,32767,false,0,{},0,0,0,false,-1,0,0,false };
		}
		task_reset = false;
		resource_change = false;
	}
	out.close();
	///////////////////////////
	// Print out ICPP Summary //
	///////////////////////////
	for (unsigned int i = 0; i < tasks.size(); i++) {
		string filename = "ICPP/output_" + std::to_string(i) + ".txt";
		outputs.open(filename);
		outputs << "lock_instant,curr_prio,prio_inherit\n";
		for (unsigned int j = 0; j < tasks.at(i).outputs.size(); j++) {
			outputs << tasks.at(i).outputs.at(j);
		}
		outputs.close();
	}
	outputs.open("ICPP/deadline_misses.txt");
	for (unsigned int i = 0; i < tasks.size(); i++) {
		outputs << std::to_string(tasks.at(i).id) << " " << tasks.at(i).deadlines_missed << "\n";
	}
	outputs.close();
}