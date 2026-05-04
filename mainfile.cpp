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
#include <regex>

using namespace std;
using json = nlohmann::json;

typedef struct Students
{
    int student_id;
    int age;
    string name;
    string gender;
    string baekjoon_id;
    string jungol_id;
    string solved_ac_tier; // 백준 티어
    string jungol_tier;    // 정올 티어 (이제 백준과 똑같은 이름으로 나옵니다!)
} stu;

map<int, stu> students_map;

// --- [CURL & API 유틸리티] ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string call_api(string url)
{
    CURL* curl;
    CURLcode res;
    string readBuffer;
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

        // 👇 VS2022 환경의 깐깐한 HTTPS 인증서 검사 무력화 (정올 뚫기 핵심!)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

string call_supabase(string url, string method, string post_data = "")
{
    CURL* curl;
    CURLcode res;
    string readBuffer;

    string anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inl4dWtnZ29ycHFzcnd1cmJhbXh2Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzU2NzUyOTQsImV4cCI6MjA5MTI1MTI5NH0.ypp6-CvVEgJkeZjxvJIL2pGNdx15Fift699kchB5cZk";

    curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("apikey: " + anon_key).c_str());
        headers = curl_slist_append(headers, ("Authorization: Bearer " + anon_key).c_str());
        headers = curl_slist_append(headers, "Prefer: resolution=merge-duplicates");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

        if (!post_data.empty())
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    return readBuffer;
}

// 기존 승훈님의 티어 변환 알고리즘 (백준, 정올 공용으로 사용합니다!)
string convert_tier(int level)
{
    if (level <= 0) return "Unrated";
    string colors[] = { "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Ruby", "Master" };
    int color_idx = (level - 1) / 5;
    int rank = 5 - (level - 1) % 5;
    if (color_idx > 5) return "Master";
    return colors[color_idx] + " " + to_string(rank);
}

// --- [티어 조회 엔진 2종] ---

// 1. 백준 (Solved.ac) 티어 조회
string get_tier_from_api(string handle)
{
    string url = "https://solved.ac/api/v3/user/show?handle=" + handle;
    string response = call_api(url);
    if (response.empty()) return "Unknown";
    try
    {
        auto data = json::parse(response);
        return convert_tier(data["tier"]);
    }
    catch (...)
    {
        return "솔브닥 미연동";
    }
}

// 2. 정올 크롤링 조회 (이제 백준식 이름으로 변환해서 반환합니다!)
string get_tier_from_jungol(string jungol_id)
{
    if (jungol_id == "None" || jungol_id == "") return "Unknown";
    string url = "https://jungol.co.kr/account/" + jungol_id;
    string html = call_api(url);
    if (html.empty()) return "Unknown";

    try
    {
        smatch match_rank, match_tier;
        regex rgx_rank("rank:(\\d+)");
        regex rgx_tier("tier:(\\d+)");

        if (regex_search(html, match_rank, rgx_rank) && regex_search(html, match_tier, rgx_tier))
        {
            string rank_str = match_rank[1].str();
            int tier_val = stoi(match_tier[1].str()); // 정올 숫자 레벨 추출

            // 핵심: 정올 레벨을 솔브닥 방식으로 강제 변환!
            string converted_tier = convert_tier(tier_val);

            // 예: "Bronze 5 (41914위)" 형태로 리턴
            return converted_tier + " (" + rank_str + "위)";
        }
        else
        {
            return "정올 미연동/비공개";
        }
    }
    catch (...)
    {
        return "파싱 에러";
    }
}

// --- [문제 추천] ---

void recommend_problems(string handle)
{
    cout << "\n[백준 데이터 기반 맞춤 문제 분석 중...]\n";
    string url = "https://solved.ac/api/v3/search/problem?query=solved_by%3A" + handle + "&sort=level&direction=desc";
    string response = call_api(url);

    try
    {
        auto data = json::parse(response);
        int total_solved = data["count"];

        if (total_solved == 0)
        {
            cout << "푼 문제가 없습니다. 문제를 더 풀고 오세요.\n";
            return;
        }

        int target_rank = min(total_solved - 1, 10);
        int page = (target_rank / 20) + 1;
        int item_idx = target_rank % 20;

        string paged_url = url + "&page=" + to_string(page);
        auto paged_data = json::parse(call_api(paged_url));

        if (paged_data["items"].size() <= (size_t)item_idx) item_idx = (int)paged_data["items"].size() - 1;

        int median_level = paged_data["items"][item_idx]["level"];
        cout << "당신의 상위권 주력 구간(Median): " << convert_tier(median_level) << "\n";

        string rec_url = "https://solved.ac/api/v3/search/problem?query=tier%3A" + to_string(median_level) + "%20-solved_by%3A" + handle;
        auto rec_data = json::parse(call_api(rec_url));
        auto& items = rec_data["items"];

        if (items.empty()) median_level++;

        vector<int> indices(items.size());
        iota(indices.begin(), indices.end(), 0);
        shuffle(indices.begin(), indices.end(), mt19937{ random_device{}() });

        cout << "--- 맞춤 추천 문제 ---\n";
        for (int i = 0; i < min(5, (int)items.size()); ++i)
        {
            auto& prob = items[indices[i]];
            cout << "[" << prob["problemId"] << "] " << prob["titleKo"]
                << " (" << convert_tier(prob["level"]) << ")\n";
        }
    }
    catch (const exception& e)
    {
        cout << "분석 중 오류 발생: " << e.what() << "\n";
    }
}

// --- [DB 관리] ---

void save_data_to_db(const stu& s)
{
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students";
    json j = {
        {"student_id", s.student_id},
        {"name", s.name},
        {"age", s.age},
        {"gender", s.gender},
        {"baekjoon_id", s.baekjoon_id},
        {"jungol_id", s.jungol_id},
        {"solved_ac_tier", s.solved_ac_tier},
        {"jungol_tier", s.jungol_tier}
    };
    call_supabase(url, "POST", j.dump());
}

void update_data_in_db(const stu& s)
{
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?student_id=eq." + to_string(s.student_id);
    json j = {
        {"name", s.name},
        {"age", s.age},
        {"gender", s.gender},
        {"baekjoon_id", s.baekjoon_id},
        {"jungol_id", s.jungol_id},
        {"solved_ac_tier", s.solved_ac_tier},
        {"jungol_tier", s.jungol_tier}
    };
    call_supabase(url, "PATCH", j.dump());
}

void delete_data_from_db(int id)
{
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?student_id=eq." + to_string(id);
    call_supabase(url, "DELETE", "");
}

// --- [학생 관리 함수] ---

void register_students()
{
    stu s;
    cout << "학생 ID: "; cin >> s.student_id;
    cout << "학생 이름: "; cin >> s.name;
    cout << "학생 나이: "; cin >> s.age;
    cout << "학생 성별: "; cin >> s.gender;
    cout << "백준 아이디: "; cin >> s.baekjoon_id;
    cout << "정올 아이디 (없으면 None): "; cin >> s.jungol_id;

    cout << "티어 조회 중...\n";
    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    s.jungol_tier = get_tier_from_jungol(s.jungol_id);

    cout << "[백준] " << s.solved_ac_tier << "\n";
    cout << "[정올] " << s.jungol_tier << "\n";

    students_map[s.student_id] = s;
    save_data_to_db(s);
}

string get_recommendation_string(string handle)
{
    string result = "\n**[오늘의 맞춤 추천 문제]**\n";
    string url = "https://solved.ac/api/v3/search/problem?query=solved_by%3A" + handle + "&sort=level&direction=desc";

    try
    {
        auto data = json::parse(call_api(url));
        if (data["count"] == 0) return result + "푼 문제가 없어 추천이 불가능합니다.\n";

        int total_solved = data["count"];
        int target_rank = min(total_solved - 1, 10);
        int page = (target_rank / 20) + 1;
        int item_idx = target_rank % 20;

        string paged_url = url + "&page=" + to_string(page);
        auto paged_data = json::parse(call_api(paged_url));

        if (paged_data["items"].size() <= (size_t)item_idx) item_idx = (int)paged_data["items"].size() - 1;

        int median_level = paged_data["items"][item_idx]["level"];
        result += "주력 구간: **" + convert_tier(median_level) + "**\n";

        string rec_url = "https://solved.ac/api/v3/search/problem?query=tier%3A" + to_string(median_level) + "%20-solved_by%3A" + handle;
        auto rec_data = json::parse(call_api(rec_url));
        auto& items = rec_data["items"];

        if (items.empty()) median_level++;

        vector<int> indices(items.size());
        iota(indices.begin(), indices.end(), 0);
        shuffle(indices.begin(), indices.end(), mt19937{ random_device{}() });

        for (int i = 0; i < min(5, (int)items.size()); ++i)
        {
            auto& prob = items[indices[i]];
            result += "[" + to_string((int)prob["problemId"]) + "] " + (string)prob["titleKo"] + " (" + convert_tier(prob["level"]) + ")\n";
        }
    }
    catch (...)
    {
        return result + "문제 분석 중 오류 발생.\n";
    }
    return result;
}

void send_discord_profile_and_rec(const stu& s, string rec_str)
{
    string webhook_url = "https://discordapp.com/api/webhooks/1491991296242487360/k5tDO-4atCmI3rWn9Bbi_6G-xZwArWlnAqzQSZT0YyJluZsycGURfwMcq7GTE9otCl0D";

    string message = "**[O(1) 부원 정보 조회]**\n";
    message += "**이름:** " + s.name + " (" + to_string(s.student_id) + ")\n";
    message += "**백준:** " + s.solved_ac_tier + " (" + s.baekjoon_id + ")\n";
    message += "**정올:** " + s.jungol_tier + " (" + s.jungol_id + ")\n";
    message += rec_str;

    json j = { {"content", message} };
    string post_data = j.dump();

    CURL* curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void search_students()
{
    int id;
    cout << "조회할 학번: "; cin >> id;
    if (students_map.find(id) != students_map.end())
    {
        stu& s = students_map[id];
        cout << "--- " << s.name << " 학생 정보 ---\n";
        cout << "ID: " << s.student_id << " | 나이: " << s.age << " | 성별: " << s.gender << "\n";
        cout << "[백준] " << s.solved_ac_tier << " (ID: " << s.baekjoon_id << ")\n";
        cout << "[정올] " << s.jungol_tier << " (ID: " << s.jungol_id << ")\n";

        cout << "\n백준 맞춤 문제를 추천받으시겠습니까? (1: Yes, 0: No): ";
        int rec; cin >> rec;

        string rec_str = "";
        if (rec == 1)
        {
            recommend_problems(s.baekjoon_id);
            rec_str = get_recommendation_string(s.baekjoon_id);
        }

        cout << "\n이 정보를 디스코드로 전송하시겠습니까? (1: Yes, 0: No): ";
        int send_discord; cin >> send_discord;
        if (send_discord == 1)
        {
            cout << "디스코드로 전송 중...\n";
            send_discord_profile_and_rec(s, rec_str);
            cout << "전송 완료!\n";
        }
    }
    else
    {
        cout << "정보가 없습니다.\n";
    }
}

void modify_students()
{
    int id;
    cout << "수정할 학번: "; cin >> id;
    if (students_map.find(id) == students_map.end())
    {
        cout << "등록된 학생이 아닙니다.\n";
        return;
    }
    stu& s = students_map[id];
    cout << "새 이름: "; cin >> s.name;
    cout << "새 나이: "; cin >> s.age;
    cout << "새 성별: "; cin >> s.gender;
    cout << "새 백준 ID: "; cin >> s.baekjoon_id;
    cout << "새 정올 ID (숫자입력): "; cin >> s.jungol_id;

    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    s.jungol_tier = get_tier_from_jungol(s.jungol_id);
    cout << "정보 및 티어가 모두 수정되었습니다.\n";
    update_data_in_db(s);
}

void delete_students()
{
    int id;
    cout << "삭제할 학번: "; cin >> id;
    if (students_map.erase(id))
    {
        cout << "삭제 완료.\n";
        delete_data_from_db(id);
    }
    else cout << "찾을 수 없습니다.\n";
}

void filter_students()
{
    cout << "\n--- 필터링 옵션 ---\n";
    cout << "1. 성별로 검색\n";
    cout << "2. 학년으로 검색\n";
    cout << "선택: ";

    int filter_choice;
    cin >> filter_choice;

    if (filter_choice == 1)
    {
        string target_gender;
        cout << "검색할 성별 입력 (예: 남, 여): "; cin >> target_gender;
        int count = 0;
        for (const auto& item : students_map)
        {
            const stu& s = item.second;
            if (s.gender == target_gender)
            {
                cout << "[" << s.student_id << "] " << s.name << " | 백준: " << s.solved_ac_tier << " | 정올: " << s.jungol_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else if (filter_choice == 2)
    {
        char target_grade;
        cout << "검색할 학년 입력 (예: 1, 2, 3): "; cin >> target_grade;
        int count = 0;
        for (const auto& item : students_map)
        {
            const stu& s = item.second;
            string id_str = to_string(s.student_id);
            if (id_str[0] == target_grade)
            {
                cout << "[" << s.student_id << "] " << s.name << " | 백준: " << s.solved_ac_tier << " | 정올: " << s.jungol_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else cout << "잘못된 선택입니다.\n";
}

void load_data_from_db()
{
    cout << "\n[클라우드 데이터베이스에서 정보를 불러오는 중...]\n";
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?select=*";
    string response = call_supabase(url, "GET");

    if (response.empty() || response == "[]")
    {
        cout << "[알림] DB에 저장된 데이터가 없습니다.\n";
        return;
    }

    try
    {
        auto data = json::parse(response);
        students_map.clear();

        for (const auto& item : data)
        {
            stu s;
            s.student_id = item["student_id"];
            s.name = item["name"];
            s.age = item["age"];
            s.gender = item["gender"];
            s.baekjoon_id = item.contains("baekjoon_id") && !item["baekjoon_id"].is_null() ? item["baekjoon_id"] : "None";
            s.jungol_id = item.contains("jungol_id") && !item["jungol_id"].is_null() ? item["jungol_id"] : "None";
            s.solved_ac_tier = item.contains("solved_ac_tier") && !item["solved_ac_tier"].is_null() ? item["solved_ac_tier"] : "Unknown";
            s.jungol_tier = item.contains("jungol_tier") && !item["jungol_tier"].is_null() ? item["jungol_tier"] : "Unknown";

            students_map[s.student_id] = s;
        }
        cout << "성공적으로 " << students_map.size() << "명의 데이터를 동기화했습니다!\n";
    }
    catch (const exception& e)
    {
        cout << "데이터 파싱 중 오류 발생: " << e.what() << "\n";
    }
}

void send_discord_webhook(string name, string platform, string old_tier, string new_tier)
{
    string webhook_url = "https://discordapp.com/api/webhooks/1491991296242487360/k5tDO-4atCmI3rWn9Bbi_6G-xZwArWlnAqzQSZT0YyJluZsycGURfwMcq7GTE9otCl0D";
    string message = "🎉 **속보** 🎉\n[" + name + "] 학생이 **" + platform + "**에서 **" + old_tier + "** -> **" + new_tier + "**(으)로 승급했습니다!";

    json j = { {"content", message} };
    string post_data = j.dump();

    CURL* curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());

        // 디스코드도 깐깐하게 막는 경우가 있어서 인증서 우회 적용
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void update_all_tiers()
{
    cout << "\n[전체 학생 통합 티어 갱신 시작...]\n";
    int count = 0;

    for (auto& item : students_map)
    {
        stu& s = item.second;
        cout << s.name << " 조회 중... ";
        bool changed = false;

        // 백준 체크
        string new_bj = get_tier_from_api(s.baekjoon_id);
        if (new_bj != "Unknown" && new_bj != "솔브닥 미연동" && s.solved_ac_tier != new_bj)
        {
            cout << "\n  -> [백준 변동] " << s.solved_ac_tier << " -> " << new_bj;
            send_discord_webhook(s.name, "백준", s.solved_ac_tier, new_bj);
            s.solved_ac_tier = new_bj;
            changed = true;
        }

        // 정올 체크 (방어 로직 포함: 기존 "Tier X"가 DB에 있으면 덮어씁니다)
        string new_jg = get_tier_from_jungol(s.jungol_id);
        if (new_jg != "Unknown" && new_jg != "정올 미연동/비공개" && s.jungol_tier != new_jg)
        {
            cout << "\n  -> [정올 변동] " << s.jungol_tier << " -> " << new_jg;
            // 최초 변환 시 디스코드 폭탄을 막으려면 알림은 잠깐 꺼둬도 됩니다
            // send_discord_webhook(s.name, "정올", s.jungol_tier, new_jg); 
            s.jungol_tier = new_jg;
            changed = true;
        }

        if (changed)
        {
            update_data_in_db(s);
            count++;
        }
        else cout << "변동 없음.";

        cout << "\n";
        Sleep(500);
    }

    if (count > 0) cout << "\n총 " << count << "명의 티어가 갱신 및 저장되었습니다!\n";
    else cout << "\n새롭게 변동된 티어가 없습니다.\n";
}

// 랭킹 점수 공통 로직! (이제 정올/백준 둘 다 이 함수 하나로 계산됩니다)
int get_tier_score(string tier_str)
{
    if (tier_str == "솔브닥 미연동" || tier_str == "Unknown" || tier_str == "Not Found" || tier_str == "정올 미연동/비공개") return -1;
    if (tier_str == "Unrated") return 0;
    if (tier_str == "Master") return 31;

    stringstream ss(tier_str);
    string color; int rank = 5;
    ss >> color >> rank;

    int base = 0;
    if (color == "Bronze") base = 0;
    else if (color == "Silver") base = 5;
    else if (color == "Gold") base = 10;
    else if (color == "Platinum") base = 15;
    else if (color == "Diamond") base = 20;
    else if (color == "Ruby") base = 25;

    return base + (6 - rank);
}

// 괄호 안에 있는 정올 순위 파싱 전용
int get_jungol_tier_score(string tier_str, int& rank_out)
{
    rank_out = 9999999;

    // DB에 예전 "Tier 2"가 남아있을 경우 터지지 않게 보호
    if (tier_str.find("Tier ") == 0)
    {
        int tier_val = 0;
        sscanf_s(tier_str.c_str(), "Tier %d (%d위)", &tier_val, &rank_out);
        return tier_val;
    }

    // 새로운 솔브닥 형식 "Bronze 5 (41914위)" 계산
    int score = get_tier_score(tier_str);

    size_t start = tier_str.find("(");
    size_t end = tier_str.find("위)");
    if (start != string::npos && end != string::npos)
    {
        try {
            string rank_str = tier_str.substr(start + 1, end - start - 1);
            rank_out = stoi(rank_str);
        }
        catch (...) {}
    }

    return score;
}

void print_ranking()
{
    cout << "\n어떤 랭킹을 출력하시겠습니까?\n";
    cout << "1. 🏆 백준 (Solved.ac) 랭킹\n";
    cout << "2. 🚀 정올 (JUNGOL) 랭킹\n";
    cout << "선택: ";
    int rank_choice; cin >> rank_choice;

    vector<stu> sorted_students;
    for (const auto& item : students_map) sorted_students.push_back(item.second);

    cout << "\n==========================================\n";
    if (rank_choice == 1)
    {
        cout << "   [O(1) 명예의 전당 - 백준 티어 랭킹]\n";
        sort(sorted_students.begin(), sorted_students.end(), [](const stu& a, const stu& b) {
            return get_tier_score(a.solved_ac_tier) > get_tier_score(b.solved_ac_tier);
            });

        int rank = 1;
        for (const auto& s : sorted_students) {
            cout << rank << "위: [" << s.name << "] " << s.solved_ac_tier << " (" << s.baekjoon_id << ")\n";
            rank++;
        }
    }
    else if (rank_choice == 2)
    {
        cout << "   [O(1) 명예의 전당 - 정올 공식 랭킹]\n";
        sort(sorted_students.begin(), sorted_students.end(), [](const stu& a, const stu& b) {
            int rank_a, rank_b;
            int score_a = get_jungol_tier_score(a.jungol_tier, rank_a);
            int score_b = get_jungol_tier_score(b.jungol_tier, rank_b);
            if (score_a != score_b) return score_a > score_b;
            return rank_a < rank_b; // 점수가 같으면 등수가 작은(높은) 사람이 위로!
            });

        int rank = 1;
        for (const auto& s : sorted_students) {
            cout << rank << "위: [" << s.name << "] " << s.jungol_tier << " (정올 ID: " << s.jungol_id << ")\n";
            rank++;
        }
    }
    else cout << "잘못된 선택입니다.\n";
    cout << "==========================================\n";
}

void print_all_students()
{
    cout << "\n--- 전체 학생 목록 ---\n";
    for (const auto& item : students_map)
    {
        const stu& s = item.second;
        cout << "[" << s.student_id << "] " << s.name << " (" << s.age << "세, " << s.gender << ")\n";
        cout << "  -> 백준: " << s.solved_ac_tier << " | 정올: " << s.jungol_tier << "\n";
    }
}

int main()
{
    system("chcp 65001");
    curl_global_init(CURL_GLOBAL_ALL);

    load_data_from_db();

    // 시작할 때 무조건 한 번 싹 돌아서 옛날 데이터를 새 데이터로 덮어씌워줍니다!
    if (!students_map.empty()) update_all_tiers();
    else cout << "[알림] 등록된 학생이 없어 자동 갱신을 건너뜁니다.\n";

    while (1)
    {
        cout << "\n1.등록 2.검색 3.수정 4.삭제 5.전체출력 6.필터링 7.랭킹출력 8.종료\n선택: ";
        int choice; cin >> choice;

        if (choice == 1) register_students();
        else if (choice == 2) search_students();
        else if (choice == 3) modify_students();
        else if (choice == 4) delete_students();
        else if (choice == 5) print_all_students();
        else if (choice == 6) filter_students();
        else if (choice == 7) print_ranking();
        else if (choice == 8) break;
        else cout << "잘못된 입력입니다.\n";
    }

    curl_global_cleanup();
    return 0;
}