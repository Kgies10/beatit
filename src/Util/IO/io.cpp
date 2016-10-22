#include "Util/IO/io.hpp"
#include <sys/stat.h>
#include "libmesh/parallel.h"

namespace BeatIt
{



void printBanner( std::ostream& cout)
{
	cout << "\n\t .______    _______     ___   .___________.    __  .___________.";
	cout << "\n\t |   _  \\  |   ____|   /   \\  |           |   |  | |           |";
	cout << "\n\t |  |_)  | |  |__     /  ^  \\ `---|  |----`   |  | `---|  |---- ";
	cout << "\n\t |   _  <  |   __|   /  /_\\  \\    |  |        |  |     |  |     ";
	cout << "\n\t |  |_)  | |  |____ /  _____  \\   |  |        |  |     |  |     ";
	cout << "\n\t |______/  |_______/__/     \\__\\  |__|        |__|     |__| 	 ";
	cout << "\n";
}


void saveData( double time,
		       std::vector<double>& var,
			   std::ostream& output)
{
	output << time;
	for(const auto& v : var)
	{
		output << " " << v;
	}
	output << "\n";
}


bool  readList(std::string& list, std::vector<std::string>& container )
{
	auto beg = list.begin();
	auto end = list.end();
	namespace qi = boost::spirit::qi;
	namespace ascii = boost::spirit::ascii;
	bool ok = qi::phrase_parse(beg, end,  (*~qi::char_(",")) % ',', ascii::blank, container);
	return ok;
}

void createOutputFolder(const libMesh::Parallel::Communicator & comm,
		                                          std::string& output_folder )
{
	struct stat out_dir;
    if( stat(&output_folder[0],&out_dir) != 0  )
    {
        if ( comm.rank() == 0 )
        {
            mkdir ( output_folder.c_str(), 0777 );
        }
    }
}




} // namespace BeatIt


