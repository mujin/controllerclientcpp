#include <boost/property_tree/json_parser.hpp>
#include <iostream>

using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "usage: ./mujinparsepublishedstate jsonfile\n";
    }
    ifstream ifs(argv[1]);
    stringstream sout;
    sout << ifs.rdbuf();
    cout << "parsing string\n" << sout.str();
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(sout, pt); // just check no exception is thrown

    cout << "\nsucceeded!\n";
    return 0;
}
