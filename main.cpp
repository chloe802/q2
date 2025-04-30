#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <chrono>
#include <ctime>
#include <bitset>
#include <algorithm>
#include <iomanip>

using namespace std;

// constant for router id (my student id last 4 digits)
const int ROUTER_ID = 8581;

// struct to represent each forwarding table entry
struct ForwardingEntry {
    string prefix;
    int prefixLength;
    string nextHop;
};

// struct for packet info (priority and flow used for scheduling)
struct Packet {
    string destIP;
    int priority;
    int flowID;
    int arrivalTime;
};

// converts a string ip (like 192.168.1.1) into a 32-bit binary string
string ipToBinary(const string& ip) {
    stringstream ss(ip);
    string segment;
    vector<string> tokens;
    while (getline(ss, segment, '.')) tokens.push_back(segment);

    string binary = "";
    for (const auto& token : tokens) {
        binary += bitset<8>(stoi(token)).to_string();
    }
    return binary;
}

/*
    challenge i ran into: i was comparing ip addresses using decimals at first,
    which didn't work for specific subnets like /30 or /31. i fixed it by using
    bitset to convert everything to binary (lines 29–38) and using substr to
    compare only the bits that matter. the router id tie-breaker also kicks in
    when two prefixes of the same length match (line 59).
*/

// does the longest prefix match using binary comparison
string longestPrefixMatch(const string& ip, const vector<ForwardingEntry>& table) {
    string binIP = ipToBinary(ip);
    int maxMatch = -1;
    string chosenNextHop = "";
    vector<string> candidates;

    for (const auto& entry : table) {
        string binPrefix = ipToBinary(entry.prefix).substr(0, entry.prefixLength);
        if (binIP.substr(0, entry.prefixLength) == binPrefix) {
            if (entry.prefixLength > maxMatch) {
                maxMatch = entry.prefixLength;
                candidates.clear();
                candidates.push_back(entry.nextHop);
            } else if (entry.prefixLength == maxMatch) {
                candidates.push_back(entry.nextHop);
            }
        }
    }

    // breaks ties using my router id
    if (candidates.size() > 1) {
        return candidates[ROUTER_ID % candidates.size()];
    } else if (!candidates.empty()) {
        return candidates[0];
    }
    return "DROP";
}

// round robin scheduler - cycles through flows evenly
class RoundRobin {
    map<int, queue<Packet>> flows;
    vector<int> flowOrder;
    int currentIndex = 0;

public:
    // adds packet to the right flow queue
    void addPacket(const Packet& p) {
        if (flows[p.flowID].empty()) flowOrder.push_back(p.flowID);
        flows[p.flowID].push(p);
    }

    // gets the next packet using round robin
    Packet getNextPacket() {
        for (int i = 0; i < flowOrder.size(); ++i) {
            currentIndex = (currentIndex + 1) % flowOrder.size();
            int flow = flowOrder[currentIndex];
            if (!flows[flow].empty()) {
                Packet pkt = flows[flow].front();
                flows[flow].pop();
                return pkt;
            }
        }
        return {"", -1, -1, -1};
    }
};

// weighted fair queueing scheduler - gives more priority to higher weight
class WeightedFairQueue {
    map<int, queue<Packet>> flows;
    map<int, int> weights = {{0, 1}, {1, 2}, {2, 3}}; // example weights

public:
    // adds packet to queue based on priority
    void addPacket(const Packet& p) {
        flows[p.priority].push(p);
    }

    // returns the next packet based on weighted priority
    Packet getNextPacket() {
        for (auto& [priority, q] : flows) {
            if (!q.empty()) {
                Packet pkt = q.front();
                q.pop();
                return pkt;
            }
        }
        return {"", -1, -1, -1};
    }
};

// detects head-of-line blocking if low priority packet is stuck at front
void monitorHOL(queue<Packet>& line) {
    if (!line.empty() && line.front().priority < 1) {
        cout << "HOL Blocking Detected at " << chrono::system_clock::to_time_t(chrono::system_clock::now()) << endl;
    }
}

int main() {
    // sets up the forwarding table (15 entries with diff prefix lengths)
    vector<ForwardingEntry> table = {
            {"192.168.1.0", 24, "A"}, {"192.168.0.0", 16, "B"}, {"10.0.0.0", 8, "C"},
            {"172.16.0.0", 12, "D"}, {"192.168.1.128", 25, "E"}, {"192.168.1.64", 26, "F"},
            {"192.168.1.32", 27, "G"}, {"192.168.1.16", 28, "H"}, {"192.168.1.8", 29, "I"},
            {"192.168.1.4", 30, "J"}, {"192.168.1.2", 31, "K"}, {"192.168.1.1", 32, "L"},
            {"8.8.8.0", 24, "M"}, {"1.1.1.0", 24, "N"}, {"192.0.2.0", 24, "O"}
    };

    // prints out the forwarding table
    cout << "Forwarding Table Entries (" << table.size() << " total):\n";
    cout << left << setw(20) << "Prefix" << setw(15) << "Length" << setw(10) << "Next Hop" << endl;
    cout << "-----------------------------------------------------\n";
    for (const auto& entry : table) {
        cout << left << setw(20) << entry.prefix << setw(15) << entry.prefixLength << setw(10) << entry.nextHop << endl;
    }
    cout << endl;

    // logs the current timestamp for congestion simulation
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    cout << "Running simulation at timestamp: " << ctime(&now) << endl;

    // tests the prefix matching logic
    string testIP = "192.168.1.5";
    cout << "Destination IP: " << testIP << " matched to next hop: " << longestPrefixMatch(testIP, table) << endl;

    // sets up both schedulers and the hol queue
    RoundRobin rr;
    WeightedFairQueue wfq;
    queue<Packet> holTest;

    // creates sample packets and adds them to all schedulers
    for (int i = 0; i < 6; ++i) {
        Packet p = {"192.168.1.5", i % 3, i % 2, static_cast<int>(i)};
        rr.addPacket(p);
        wfq.addPacket(p);
        holTest.push(p);
    }

    // prints round robin results
    cout << "\nRound Robin Scheduler Output:" << endl;
    for (int i = 0; i < 3; ++i) {
        Packet pkt = rr.getNextPacket();
        cout << "Packet to " << pkt.destIP << " from flow " << pkt.flowID << endl;
    }

    // prints weighted fair queue results
    cout << "\nWeighted Fair Queue Output:" << endl;
    for (int i = 0; i < 3; ++i) {
        Packet pkt = wfq.getNextPacket();
        cout << "Packet to " << pkt.destIP << " with priority " << pkt.priority << endl;
    }

    // check for head-of-line blocking
    cout << "\nMonitoring HOL Blocking..." << endl;
    monitorHOL(holTest);

    return 0;
}
