#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <ctime>
#include <numeric> 
#include <fstream>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>

using namespace std;
using json = nlohmann::json;

typedef struct Students {
    int student_id;
    int age;
    string name;
    string gender;
    string baekjoon_id;
    string solved_ac_tier;
} stu;

map<int, stu> students_map;

// --- [CURL & API 유틸리티] ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string call_api(string url) {
    CURL* curl;
    CURLcode res;
    string readBuffer;
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

string convert_tier(int level) {
    if (level <= 0) return "Unrated";
    string colors[] = { "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Ruby", "Master" };
    int color_idx = (level - 1) / 5;
    int rank = 5 - (level - 1) % 5;
    if (color_idx > 5) return "Master";
    return colors[color_idx] + " " + to_string(rank);
}

// --- [핵심 기능 구현] ---

string get_tier_from_api(string handle) {
    string url = "https://solved.ac/api/v3/user/show?handle=" + handle;
    string response = call_api(url);
    if (response.empty()) return "Unknown";
    try {
        auto data = json::parse(response);
        return convert_tier(data["tier"]);
    }
    catch (...) {
        return "Not Found";
    }
}

void recommend_problems(string handle) {
    cout << "\n[문제 추천 분석 중...]\n";

    string url = "https://solved.ac/api/v3/search/problem?query=solved_by%3A" + handle + "&sort=level&direction=desc";
    string response = call_api(url);

    try {
        auto data = json::parse(response);
        int total_solved = data["count"];

        if (total_solved == 0) {
            cout << "푼 문제가 없습니다. 문제를 더 풀고 오세요.\n";
            return;
        }

        //int analysis_limit = min(total_solved, 100);
        //int target_rank = (analysis_limit - 1) / 2;
        int target_rank = min(total_solved - 1, 10);

        int page = (target_rank / 20) + 1;
        int item_idx = target_rank % 20;

        string paged_url = url + "&page=" + to_string(page);
        string paged_response = call_api(paged_url);
        auto paged_data = json::parse(paged_response);

        if (paged_data["items"].size() <= (size_t)item_idx) {
            item_idx = (int)paged_data["items"].size() - 1;
        }

        int median_level = paged_data["items"][item_idx]["level"];
        cout << "당신의 상위권 주력 구간(Median): " << convert_tier(median_level) << "\n";

        string rec_url = "https://solved.ac/api/v3/search/problem?query=tier%3A" + to_string(median_level) + "%20-solved_by%3A" + handle;
        string rec_response = call_api(rec_url);
        auto rec_data = json::parse(rec_response);
        auto& items = rec_data["items"];

        if (items.empty()) {
            cout << "이 구간에서 풀지 않은 문제가 없습니다.\n";
            median_level++;
        }

        vector<int> indices(items.size());
        iota(indices.begin(), indices.end(), 0);
        shuffle(indices.begin(), indices.end(), mt19937{ random_device{}() });

        cout << "--- 추천 문제 ---\n";
        for (int i = 0; i < min(5, (int)items.size()); ++i) {
            auto& prob = items[indices[i]];
            cout << "[" << prob["problemId"] << "] " << prob["titleKo"]
                << " (" << convert_tier(prob["level"]) << ")\n";
        }

        //cout << "\n🔥 챌린지 과제: " << convert_tier(median_level + 1) << " 문제도 도전해 보세요!\n";

    }
    catch (const exception& e) {
        cout << "분석 중 오류 발생: " << e.what() << "\n";
    }
}

// --- [학생 관리 함수] ---

void register_students() {
    stu s;
    cout << "학생 ID: "; cin >> s.student_id;
    cout << "학생 이름: "; cin >> s.name;
    cout << "학생 나이: "; cin >> s.age;
    cout << "학생 성별: "; cin >> s.gender;
    cout << "백준 아이디: "; cin >> s.baekjoon_id;

    cout << "솔브닥 티어 조회 중...\n";
    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    cout << "조회 완료: " << s.solved_ac_tier << "\n";

    students_map[s.student_id] = s;
}

void search_students() {
    int id;
    cout << "조회할 학번: "; cin >> id;
    if (students_map.find(id) != students_map.end()) {
        stu& s = students_map[id];
        cout << "--- " << s.name << " 학생 정보 ---\n";
        //cout << "ID: " << s.student_id << " | 성별: " << s.gender << "\n";
        cout << "ID: " << s.student_id << "\n" << "이름: " << s.name << "\n";
        cout << "나이: " << s.age << "\n" << "성별: " << s.gender << "\n";
        cout << "백준: " << s.baekjoon_id << " (" << s.solved_ac_tier << ")\n";

        cout << "\n문제를 추천받으시겠습니까? (1: Yes, 0: No): ";
        int rec; cin >> rec;
        if (rec == 1) recommend_problems(s.baekjoon_id);
    }
    else {
        cout << "정보가 없습니다.\n";
    }
}

void modify_students() {
    int id;
    cout << "수정할 학번: "; cin >> id;
    if (students_map.find(id) == students_map.end()) {
        cout << "등록된 학생이 아닙니다.\n";
        return;
    }
    stu& s = students_map[id];
    cout << "새 이름: "; cin >> s.name;
    cout << "새 나이: "; cin >> s.age;
    cout << "새 성별: "; cin >> s.gender;
    cout << "새 백준 ID: "; cin >> s.baekjoon_id;

    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    cout << "정보 및 티어가 수정되었습니다.\n";
}

void delete_students() {
    int id;
    cout << "삭제할 학번: "; cin >> id;
    if (students_map.erase(id)) cout << "삭제 완료.\n";
    else cout << "찾을 수 없습니다.\n";
}

void print_all_students() {
    cout << "\n--- 전체 학생 목록 ---\n";
    // map은 이미 Key(학번) 기준으로 정렬되어 있습니다!
    for (const auto& item : students_map) {
        const stu& s = item.second;
        cout << "[" << s.student_id << "] " << s.name
            << " (" << s.age << "세, " << s.gender << ") - " << s.solved_ac_tier << "\n";
    }
}

void filter_students() {
    cout << "\n--- 필터링 옵션 ---\n";
    cout << "1. 성별로 검색\n";
    cout << "2. 학년으로 검색\n";
    cout << "선택: ";

    int filter_choice;
    cin >> filter_choice;

    if (filter_choice == 1) {
        string target_gender;
        cout << "검색할 성별 입력 (예: 남, 여): ";
        cin >> target_gender;

        cout << "\n--- [" << target_gender << "] 학생 목록 ---\n";
        int count = 0;
        for (const auto& item : students_map) {
            const stu& s = item.second;
            if (s.gender == target_gender) {
                cout << "[" << s.student_id << "] " << s.name
                    << " (" << s.age << "세) - " << s.solved_ac_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else if (filter_choice == 2) {
        char target_grade;
        cout << "검색할 학년 입력 (예: 1, 2, 3): ";
        cin >> target_grade;

        cout << "\n--- [" << target_grade << "학년] 학생 목록 ---\n";
        int count = 0;
        for (const auto& item : students_map) {
            const stu& s = item.second;

            // 핵심 로직: int형 학번을 string으로 바꾸고 첫 번째 글자([0])를 가져옴
            string id_str = to_string(s.student_id);

            if (id_str[0] == target_grade) {
                cout << "[" << s.student_id << "] " << s.name
                    << " (" << s.gender << ") - " << s.solved_ac_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else {
        cout << "잘못된 선택입니다. 메뉴로 돌아갑니다.\n";
    }
}


void save_data() {
    ofstream myfile("students.txt");
    if (myfile.is_open()) {
        for (const auto& item : students_map) {
            const stu& s = item.second;
            // 항목 사이에 ,를 넣어 저장
            myfile << s.student_id << "," << s.name << "," << s.age << ","
                << s.gender << "," << s.baekjoon_id << "," << s.solved_ac_tier << "\n";
        }
    }
}

void load_data() {
    ifstream myfile("students.txt");
    if (!myfile.is_open()) return;

    string line;
    while (getline(myfile, line)) {
        stringstream ss(line);
        string temp;
        stu s;

        getline(ss, temp, ','); s.student_id = stoi(temp);
        getline(ss, s.name, ',');
        getline(ss, temp, ','); s.age = stoi(temp);
        getline(ss, s.gender, ',');
        getline(ss, s.baekjoon_id, ',');
        getline(ss, s.solved_ac_tier, ','); // 이제 "Gold 4"를 통째로 잘 가져옵니다!

        students_map[s.student_id] = s;
    }
}

void update_all_tiers() {
    cout << "\n[전체 학생 티어 갱신 시작...]\n";
    int count = 0;

    for (auto& item : students_map) {
        stu& s = item.second;
        cout << s.name << "(" << s.baekjoon_id << ") 조회 중... ";

        string new_tier = get_tier_from_api(s.baekjoon_id);

        if (new_tier != "Unknown" && new_tier != "Not Found") {
            if (s.solved_ac_tier != new_tier) {
                cout << "티어 변동! [" << s.solved_ac_tier << " -> " << new_tier << "]\n";
                s.solved_ac_tier = new_tier;
                count++;
            }
            else {
                cout << "변동 없음.\n";
            }
        }
        else {
            cout << "조회 실패.\n";
        }
    }

    if (count > 0) {
        save_data();
        cout << "\n총 " << count << "명의 티어가 갱신되었습니다!\n";
    }
    else {
        cout << "\n새롭게 변동된 티어가 없습니다.\n";
    }
}

int main()
{
    // 한글 깨짐 방지 및 API 초기화
    system("chcp 65001");
    curl_global_init(CURL_GLOBAL_ALL);

    // 1. 기존 데이터 불러오기
    load_data();

    // 2. 시작하자마자 전체 티어 실시간 갱신
    if (!students_map.empty()) {
        update_all_tiers();
    }
    else {
        cout << "[알림] 등록된 학생이 없어 자동 갱신을 건너뜁니다.\n";
    }

    // 3. 메뉴 루프 시작
    while (1) {
        cout << "\n1.등록 2.검색 3.수정 4.삭제 5.전체출력 6.필터링 출력 7.종료\n선택: ";
        int choice; cin >> choice;

        if (choice == 1) { register_students(); save_data(); }
        else if (choice == 2) search_students();
        else if (choice == 3) { modify_students(); save_data(); }
        else if (choice == 4) { delete_students(); save_data(); }
        else if (choice == 5) { print_all_students(); }
        else if (choice == 6) { filter_students(); }
        else if (choice == 7) break;
    }

    curl_global_cleanup();
    return 0;
}