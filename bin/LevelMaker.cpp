#include <string>
#include <iostream>
#include <vector>
#include <cctype>
#include <fstream>
#include "util/Types.h"

using namespace std;

string map_to_string(vector<vector<util::TileType>> map, vector<string> robots, vector<string> hazards, bool human_readable = false)
{
    vector<vector<string>> intermediate_map;
    string output = "";
    int row = 0;
    int col = 0;
    for (vector<util::TileType> v : map)
    {
        intermediate_map.push_back(vector<string>());
        for (util::TileType t : v)
        {

            if (!human_readable)
            {
                intermediate_map[row].push_back(to_string(static_cast<int>(t)));
                // output += to_string(static_cast<int>(t));
            }
            else if (t == util::TileType::FLOOR)
            {
                intermediate_map[row].push_back(".");
                // output += ".";
            }
            else if (t == util::TileType::WALL)
            {
                intermediate_map[row].push_back("+");
                // output += "+";
            }
            else if (t == util::TileType::EMPTY)
            {
                intermediate_map[row].push_back(" ");
                // output += " ";
            }
            else if (t == util::TileType::MODULE)
            {
                intermediate_map[row].push_back("M");
                // output += "M";
            }
            col++;
        }
        row++;
        // output += "\n";
    }

    if (human_readable)
    {
        row = 0;
        col = 0;
        // int row_size = map[0].size();
        for (string r : robots)
        {
            row = stoi(r.substr(2, r.find(",") - 2));
            col = stoi(r.substr(r.find(",") + 1, r.find(")") - r.find(",") - 1));
            intermediate_map[row][col] = r[0];
        }
        for (string h : hazards)
        {
            row = stoi(h.substr(2, h.find(",") - 2));
            col = stoi(h.substr(h.find(",") + 1, h.find(")") - h.find(",") - 1));
            intermediate_map[row][col] = 'H';
        }
        for (vector<string> v : intermediate_map)
        {
            for (string s : v)
            {
                output += s;
            }
            output += "\n";
        }
    }
    else
    {
        for (vector<string> v : intermediate_map)
        {
            for (string s : v)
            {
                output += s;
            }
            output += "\n";
        }
        for (string r : robots)
        {
            output += r;
        }
        output += "\n";
        for (string h : hazards)
        {
            output += h;
        }
    }

    return output;
}

int check_input(int num_options, bool start_at_zero = false)
{
    if (start_at_zero)
    {
        num_options -= 1;
    }
    int output = -1;
    cin >> output;
    while (output > num_options || ((output < 1 && !start_at_zero) || (output < 0 && start_at_zero)))
    {
        cout << "Invalid input. Please try again";
        cin >> output;
    }
    return output;
}

int main(int argc, char *argv[])
{
    int input = 0;
    int width = 0;
    int height = 0;
    string file_name;
    vector<vector<util::TileType>> output_matrix;
    vector<string> robots;
    vector<string> hazards;
    cout << "What type of map are you making?\n\t1: Level\n\t2: Module\n";
    input = check_input(2);
    cout << "File name (Please do not use spaces, punctuation, or special characters):\n";
    cin >> file_name;
    if (input == 1)
    {
        file_name = file_name + ".lvl";
    }
    else if (input == 2)
    {
        file_name = file_name + ".mdl";
    }
    else
    {
        cout << "Error: Invalid input made it past check_input(int)\n";
    }
    cout << "File name is: " << file_name << "\n";
    while (width < 1 || height < 1)
    {
        cout << "Enter desired width:\t";
        cin >> width;
        cout << "Enter desired height:\t";
        cin >> height;
    }
    cout << "Creating map of size " << width << "x" << height << "\n";
    output_matrix = vector<vector<util::TileType>>(height, vector<util::TileType>(width, util::TileType::FLOOR));

    bool running = true;
    bool will_output = true;
    bool fill = true;
    int x = 0;
    int y = 0;
    int temp = 0;
    int w = 0;
    int h = 0;
    util::Direction dir;
    util::TileType tile;
    string type;
    while (running)
    {
        type.clear();
        cout << "Make edits:\n\t1: Set tiles\n\t"
             << "2: Place robot\n\t"
             << "3: Place Hazard\n\t"
             << "4: Display current map\n\t"
             << "5: Finish and save\n\t"
             << "6: Quit without saving\n";
        input = check_input(6);
        switch (input)
        {
        case 1:
            cout << "What type of tile?\n\tF = Floor\n\tW = Wall\n\tM = Module\n\tV = Void\n";
            cin >> type;
            type = string(1, toupper(type[0]));
            while (type != "F" && type != "W" && type != "M" && type != "V")
            {
                cout << "Invalid response. Please enter one of the indicated characters";
                cin >> type;
            }
            if (type == "F")
            {
                tile = util::TileType::FLOOR;
            }
            else if (type == "W")
            {
                tile = util::TileType::WALL;
            }
            else if (type == "M")
            {
                tile = util::TileType::MODULE;
            }
            else if (type == "V")
            {
                tile = util::TileType::EMPTY;
            }
            cout << "What is the x coord for the top left of the rectangle?\t";
            x = check_input(output_matrix[0].size(), true);
            cout << "What is the y coord for the top left of the rectangle?\t";
            y = check_input(output_matrix.size(), true);
            cout << "What is the width of the rectangle?\t";
            w = check_input(output_matrix[0].size() - x);
            cout << "What is the height of the rectangle?\t";
            h = check_input(output_matrix[0].size() - x);
            cout << "Fill the rectangle? (Y/N)\t";
            cin >> type;
            type = string(1, toupper(type[0]));
            while (type != "Y" && type != "N")
            {
                cout << "Invalid response. Please enter one of the indicated characters";
                cin >> type;
            }
            if (type == "Y")
            {
                fill = true;
            }
            else
            {
                fill = false;
            }
            for (int row = y; row < y + h; row++)
            {
                for (int col = x; col < x + w; col++)
                {
                    if (fill || ((col == x || col == x + w - 1) || (row == y || row == y + h - 1)))
                    {
                        output_matrix[row][col] = tile;
                    }
                }
            }
            break;
        case 2:
            cout << "Enter the type of robot to be placed:\n\tM = Melee enemy\n\tR = Ranged enemy\n\tB = Boss enemy\n\tP = Player robot\n";
            cin >> type;
            type = string(1, toupper(type[0]));
            while (type != "M" && type != "R" && type != "B" && type != "P")
            {
                cout << "Invalid response. Please enter one of the indicated characters";
                cin >> type;
            }
            cout << "Enter x of robot\t";
            x = check_input(width, true);
            cout << "Enter y of robot\t";
            y = check_input(height, true);
            cout << "Enter the direction the robot should face:\n\t0: North\n\t1: East\n\t2: South\n\t3: West\n";
            temp = check_input(4, true);
            dir = static_cast<util::Direction>(temp);
            type = type[0];
            robots.push_back(type + "(" + to_string(x) + "," + to_string(y) + ")" + to_string(static_cast<int>(dir)) + ";");
            break;
        case 3:
            cout << "Hazards are not clearly defined yet. Will implement this later\n";
            break;
        case 4:
            cout << "Current map:\n";
            cout << map_to_string(output_matrix, robots, hazards, true);
            break;
        case 5:
            cout << "Outputted to " << file_name << "\n";
            running = false;
            break;
        case 6:
            cout << "Map was not saved\n";
            will_output = false;
            running = false;
            break;
        default:
            cout << "Error: Invalid input made it past check_input(int)";
            break;
        }
    }
    if (will_output)
    {
        ofstream file(file_name);
        file.clear();
        file << map_to_string(output_matrix, robots, hazards);
        file.close();
    }
    return EXIT_SUCCESS;
}