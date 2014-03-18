/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright (c) 2010, Sandia National Laboratories.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:       OutputManager
//- Description: Implementation code for the OutputManager class
//- Owner:       Brian Adams
//- Checked by:

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "dakota_global_defs.hpp"
#include "OutputManager.hpp"
#include "ProgramOptions.hpp"
#include "ProblemDescDB.hpp"
#include "ParamResponsePair.hpp"
#include "PRPMultiIndex.hpp"
#include "DakotaGraphics.hpp"
#include "ResultsManager.hpp"
#include "DakotaBuildInfo.hpp"


namespace Dakota {

// Note: MSVC requires these externs defined outside any function
extern PRPCache data_pairs;
extern Graphics dakota_graphics;
extern ResultsManager iterator_results_db;

// BMA TODO: consider removing or reimplementing
/** Heartbeat function provided by dakota_filesystem_utils; pass
    output interval in seconds, or -1 to use $DAKOTA_HEARTBEAT */
void start_dakota_heartbeat(int);


OutputManager::OutputManager():
  graph2DFlag(false), tabularDataFlag(false), resultsOutputFlag(false), 
  worldRank(0), mpirunFlag(false)
{ }


/// Only get minimal information off ProgramOptions as may be updated
/// later by broadcast
OutputManager::
OutputManager(const ProgramOptions& prog_opts, int dakota_world_rank,
	      bool dakota_mpirun_flag):
  graph2DFlag(false), tabularDataFlag(false), resultsOutputFlag(false),
  worldRank(dakota_world_rank), mpirunFlag(dakota_mpirun_flag)
{
  //  if output file specified, redirect immediately, possibly rebind later
  if (worldRank == 0 && !prog_opts.outputFile.empty()) {
    redirect_cout(prog_opts.outputFile);
  }

  if (!mpirunFlag
      // || (mpirunFlag && worldRank == 0) 
      )
    start_dakota_heartbeat(-1); // -1 ==> take interval from $DAKOTA_HEARTBEAT
}


OutputManager::~OutputManager()
{
  close_streams();
}


void OutputManager::close_streams()
{
  // check if redirection is active
  if (!coutFilename.empty() && output_ofstream.is_open()) {
    output_ofstream.close();
    dakota_cout = &std::cout;
  }
  if (!cerrFilename.empty() && error_ofstream.is_open()) {
    error_ofstream.close();
    dakota_cerr = &std::cerr;
  }

  if (restartOutputFS.is_open())
    restartOutputFS.close();

  // After completion of timings in ParallelLibrary... 
  //
  // Don't need rank-based protection as will only be closed if they
  // were opened.  Do need flag protection, otherwise dummy output
  // manager will close the global streams a second time wrongly.
  if (graph2DFlag)
    dakota_graphics.close();
  if (tabularDataFlag)
    dakota_graphics.close_tabular();

  // TODO: encapsulate tabular stream and graphics object in this
  // class if possible, or at least use this class to delegate to
  // graphics so we know that the stream was opened.
}


void OutputManager::parse(const ProblemDescDB& problem_db)
{
  graph2DFlag = problem_db.get_bool("environment.graphics");
  tabularDataFlag = problem_db.get_bool("environment.tabular_graphics_data");
  tabularDataFile = problem_db.get_string("environment.tabular_graphics_file");
  resultsOutputFlag = problem_db.get_bool("environment.results_output");
  resultsOutputFile = problem_db.get_string("environment.results_output_file");

  int db_write_precision = problem_db.get_int("environment.output_precision");
  if (db_write_precision > 0) {  // assign global write_precision
    if (db_write_precision > 16) {
      std::cout << "\nWarning: requested output_precision exceeds DAKOTA's "
		<< "internal precision;\n         resetting to 16."<< std::endl;
      write_precision = 16;
    }
    else
      write_precision = db_write_precision;
  }
}


void OutputManager::startup_message(const String& start_msg) 
{ startupMessage = start_msg; }


void OutputManager::file_tag(const String& iterator_tag) 
{ fileTag = iterator_tag; }


void OutputManager::redirect_cout(const ProgramOptions& prog_opts, 
		     bool force_cout_redirect) {
  if (force_cout_redirect || prog_opts.user_stdout_redirect())
    redirect_cout(prog_opts.stdout_filename() + fileTag);
}


void OutputManager::redirect_cerr(const ProgramOptions& prog_opts) {
  if (!prog_opts.errorFile.empty())
    redirect_cerr(prog_opts.errorFile + fileTag);
}


void OutputManager::init_resultsdb(ProgramOptions& prog_opts) {
  if (resultsOutputFlag)
    iterator_results_db.initialize(resultsOutputFile + fileTag);
}


void OutputManager::init_restart(const ProgramOptions& prog_opts) {
  bool read_restart_flag = !prog_opts.readRestartFile.empty();
  read_write_restart(read_restart_flag, 
		     prog_opts.readRestartFile,
		     prog_opts.stopRestartEvals,
		     prog_opts.write_restart_filename() + fileTag);
}


// TODO: Make release vs. stable depend on CMake settings
void OutputManager::output_version(std::ostream& os) const
{
  // Version is always output to Cout

  std::string version_info("Dakota version ");
  version_info += DakotaBuildInfo::get_release_num(); 

  // Major/interim releases:
  //version_info += "released 05/15/2013.\n";

  // Developmental/Stable releases:
  version_info += "+ developmental release.\n";

  version_info += "Subversion revision " 
    + DakotaBuildInfo::get_rev_number()
    + " built " + DakotaBuildInfo::get_build_date()
    + " " + DakotaBuildInfo::get_build_time() + ".";

  output_helper(version_info, os);
}


void OutputManager::output_startup_message(std::ostream& os) const 
{
  // Generate the startup header, now that streams are potentially
  // reassigned
  os << startupMessage << '\n'; 
  std::time_t curr_time = std::time(NULL);
  std::string pretty_time(std::asctime(std::localtime(&curr_time))); 
  // asctime appends a newline, but use the endl to force flush
  os << "Start time: " << pretty_time << std::endl;
}



void OutputManager::
output_helper(const String& message, std::ostream &os) const
{
  if (worldRank == 0)
    os << message << std::endl;
}


void OutputManager::append_restart(const ParamResponsePair& prp)
{
  // TODO: catch errors

  restartOutputArchive->operator&(prp);
  // flush is critical so we have a complete restart record should Dakota abort
  restartOutputFS.flush();
}


void OutputManager::redirect_cout(const String& new_filename)
{
  // check if there was a file name added or changed
  if (coutFilename != new_filename) {
    // close and reopen the stream to a different file
    if (output_ofstream.is_open())
      output_ofstream.close();
    output_ofstream.open(new_filename.c_str(), std::ios::out);
  }
  else {
    // BMA TODO: Review whether this is true with new ctor chain

    // If already open, just keep writing.  If not, either append
    // (created by CLH) or create new (library) file.  This is safe
    // only because the option to append is handled above, and
    // needed because a library customer may never call
    // specify_outputs_restart.
    if (!output_ofstream.is_open())
      output_ofstream.open(new_filename.c_str(), std::ios::out);
  }
  if (!output_ofstream.good()) {
    Cerr << "\nError opening output file '" << new_filename << "'" 
	 << std::endl;
    abort_handler(-1);
  }

  // assign global dakota_cout to this ofstream
  dakota_cout = &output_ofstream;
  coutFilename = new_filename;
}

void OutputManager::redirect_cerr(const String& new_filename)
{
  // check if there was a file name added or changed
  if (cerrFilename != new_filename) {
    // close and reopen the stream to a different file
    if (error_ofstream.is_open())
      error_ofstream.close();
    error_ofstream.open(new_filename.c_str(), std::ios::out);
  }
  else {
    // If already open, just keep writing.  If not, either append
    // (created by CLH) or create new (library) file.  This is safe
    // only because the option to append is handled above, and
    // needed because a library customer may never call
    // specify_outputs_restart.
    if (!error_ofstream.is_open())
      error_ofstream.open(new_filename.c_str(), std::ios::out);
  }
  if (!error_ofstream.good()) {
    Cerr << "\nError opening error file '" << new_filename << "'" 
	 << std::endl;
    abort_handler(-1);
  }

  // assign global dakota_cout to this ofstream
  dakota_cerr = &error_ofstream;
  cerrFilename = new_filename;
}


void OutputManager::read_write_restart(bool read_restart_flag,
				       const String& read_restart_filename,
				       size_t stop_restart_evals,
				       const String& write_restart_filename)
{
  // Conditionally process the evaluations from the restart file
  PRPCache read_pairs;
  if (read_restart_flag) {
    
    std::ifstream restart_input_fs(read_restart_filename.c_str(), 
				   std::ios::binary);
    if (!restart_input_fs.good()) {
      Cerr << "\nError: could not open restart file " << read_restart_filename
	   << std::endl;
      abort_handler(-1);
    }
    boost::archive::binary_iarchive restart_input_archive(restart_input_fs);

    // The -stop_restart input for restricting the number of evaluations read
    // in from the restart file is very useful when the last few evaluations in
    // a run were corrupted.  Note that the desired -stop_restart setting may
    // differ from the evaluation number in the previous run since detected 
    // duplicates are included in Interface::evalIdCntr, but are not written
    // to the restart file!
    if (stop_restart_evals)// cmd_line_handler rtns 0 if no setting
      Cout << "Stopping restart file processing at "
	   << stop_restart_evals << " evaluations." << std::endl;

    int cntr = 0;
    while ( restart_input_fs.good() && !restart_input_fs.eof() && 
            (!stop_restart_evals ||
	     cntr < stop_restart_evals) ) {
      // Use default constr. & rely on Variables::read(BiStream&)
      // & Response::read(BiStream&) to resize vars and response.
      ParamResponsePair current_pair;
      try { 
	restart_input_archive & current_pair;
      }
      catch(const boost::archive::archive_exception& e) {
	Cerr << "\nError reading restart file (boost::archive exception):\n" 
	     << e.what() << std::endl;
	abort_handler(-1);
      }
      catch(const std::string& err_msg) {
        Cout << "\nWarning reading restart file: " << err_msg << std::endl;
        break;
      }

      read_pairs.insert(current_pair); 
      ++cntr;
      Cout << "\n------------------------------------------\nRestart record "
	   << std::setw(4) << cntr << "  (evaluation id " << std::setw(4)
	   << current_pair.eval_id() << "):"
	   << "\n------------------------------------------\n" << current_pair;
      // Note: interface id printed in ParamResponsePair::write(ostream&)

      restart_input_fs.peek(); // peek to force EOF if the last record was read
    }
    restart_input_fs.close();
    Cout << "Restart file processing completed: " << cntr
	 << " evaluations retrieved.\n";
  }


  // Always write a restart log file.  Assign the write_restart stream to
  // the filename specified by the user on the dakota command line.  If a
  // write_restart file is not specified, "dakota.rst" is the default.

  // It would be desirable to suppress the creation of the restart file
  // altogether if the user has explicitly deactivated this feature; however
  // this is problematic for 2 reasons: (1) problem_db is not readily available
  // (except in init_iterator_communicators()), and (2) the "deactivate
  // restart_file" specification is linked to the interface and therefore should
  // be enforced per interface, whereas there is only one parallel lib instance.
  //if (!deactivateRestartFlag) {

  // Previously we supported append to an existing restart file.  With
  // Boost serialization, there's no easy way to append to a file (all
  // writes must occur with a single output archive instance).  This
  // also improves behavior with stop_restart, as now only the desired
  // evals are rewritten, omitting any corrupt data at the end of file.

  if (write_restart_filename == read_restart_filename)
    Cout << "Overwriting existing restart file " << write_restart_filename 
	 << std::endl;
  else
    Cout << "Writing new restart file " << write_restart_filename << std::endl;

  restartOutputFS.open(write_restart_filename.c_str(), std::ios::binary);
  restartOutputArchive = new boost::archive::binary_oarchive(restartOutputFS);

  // Write any processed records from the old restart file to the new file.
  // This prevents the situation where good data from an initial run and a 
  // restart run are in separate files.  By keeping all of the saved data in
  // 1 file, restarts can be chained together indefinitely.
  //
  // "View" the read_pairs cache as a collection ordered by eval_id

  if (!read_pairs.empty()) {
    // don't use a const iterator as PRP does not have a const serialize fn
    PRPCacheIter it, it_end = read_pairs.end();
    for (it = read_pairs.begin(); it != it_end; ++it) {
      restartOutputArchive->operator&(*it);

      // Distinguish restart evals in memory by negating their eval ids;
      // positive ids could be misleading if inconsistent with the progression
      // of a restarted run (resulting in different evaluations that share the
      // same evalInterfaceIds).  Moreover, they may not be unique within the
      // restart DB in the presence of overlay/concatenation of multiple runs.
      //   id > 0 for unique evals from current execution (in data_pairs)
      //   id = 0 for evals from file import (not in data_pairs)
      //   id < 0 for non-unique evals from restart (in data_pairs)
      int restart_eval_id = it->eval_id();
      if (restart_eval_id > 0) {
	ParamResponsePair pair(*it); // shallow vars/resp copy, deep ids copy
	pair.eval_id(-restart_eval_id);
	data_pairs.insert(pair);
      }
      else
	data_pairs.insert(*it);
    }

    // flush is critical so we have a complete restart record in case of abort
    restartOutputFS.flush();
  }

  //}
}


} //namespace Dakota
