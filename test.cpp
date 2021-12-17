#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

bool accept = false;
ifstream file_input;
string DstIP;
string cmd = "CONNECT";
void checkfirewall(){
    string input;
    while(getline(file_input,input)){
        if(input[7] == 'b' && cmd == "CONNECT"){
            input = input.substr(9);
            stringstream X(input);
            stringstream Y(DstIP);
            string permit,dst;
            int times = 0;
            while (getline(X, permit, '.') && getline(Y,dst,'.')) { 
                if(permit == "*"){
                    accept = true;
                    break;
                }
                else if(permit == dst){
                    times++;
                    continue;
                }else{
                    break;
                }
            }
            if(times == 4) accept = true;
        }
    }
}
int main(){
    file_input.open("socks.conf",ios::in);
    cin >> DstIP;
    checkfirewall();
    if(accept) cout << "accept" << endl;
    else cout << "reject" << endl;
}
