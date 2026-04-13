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

typedef struct Students
{
    int student_id;
    int age;
    string name;
    string gender;
    string baekjoon_id;
    string solved_ac_tier;
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
        //curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        // 크롬 브라우저인 척 위장하기!
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
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

    // 승훈님이 찾은 정보
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

        // 이 부분이 핵심
        // PATCH, DELETE 등을 서버에 명시적으로 알려줍니다.
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

string convert_tier(int level) 
{
    if (level <= 0) return "Unrated";
    string colors[] = { "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Ruby", "Master" };
    int color_idx = (level - 1) / 5;
    int rank = 5 - (level - 1) % 5;
    if (color_idx > 5)
    {
        return "Master";
    }
    return colors[color_idx] + " " + to_string(rank);
}

// --- [핵심 기능 구현] ---

string get_tier_from_api(string handle)
{
    string url = "https://solved.ac/api/v3/user/show?handle=" + handle;
    string response = call_api(url);
    if (response.empty())
    {
        return "Unknown";
    }
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

void recommend_problems(string handle)
{
    cout << "\n[문제 추천 분석 중...]\n";

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

        //int analysis_limit = min(total_solved, 100);
        //int target_rank = (analysis_limit - 1) / 2;
        int target_rank = min(total_solved - 1, 10);

        int page = (target_rank / 20) + 1;
        int item_idx = target_rank % 20;

        string paged_url = url + "&page=" + to_string(page);
        string paged_response = call_api(paged_url);
        auto paged_data = json::parse(paged_response);

        if (paged_data["items"].size() <= (size_t)item_idx)
        {
            item_idx = (int)paged_data["items"].size() - 1;
        }

        int median_level = paged_data["items"][item_idx]["level"];
        cout << "당신의 상위권 주력 구간(Median): " << convert_tier(median_level) << "\n";

        string rec_url = "https://solved.ac/api/v3/search/problem?query=tier%3A" + to_string(median_level) + "%20-solved_by%3A" + handle;
        string rec_response = call_api(rec_url);
        auto rec_data = json::parse(rec_response);
        auto& items = rec_data["items"];

        if (items.empty())
        {
            cout << "이 구간에서 풀지 않은 문제가 없습니다.\n";
            median_level++;
        }

        vector<int> indices(items.size());
        iota(indices.begin(), indices.end(), 0);
        shuffle(indices.begin(), indices.end(), mt19937{ random_device{}() });

        cout << "--- 추천 문제 ---\n";
        for (int i = 0; i < min(5, (int)items.size()); ++i)
        {
            auto& prob = items[indices[i]];
            cout << "[" << prob["problemId"] << "] " << prob["titleKo"]
                << " (" << convert_tier(prob["level"]) << ")\n";
        }

        //cout << "\n🔥 챌린지 과제: " << convert_tier(median_level + 1) << " 문제도 도전해 보세요!\n";

    }
    catch (const exception& e)
    {
        cout << "분석 중 오류 발생: " << e.what() << "\n";
    }
}

void save_data_to_db(const stu& s)
{
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students";

    json j = {
        {"student_id", s.student_id},
        {"name", s.name},
        {"age", s.age},
        {"gender", s.gender},
        {"baekjoon_id", s.baekjoon_id},
        {"solved_ac_tier", s.solved_ac_tier}
    };

    // 데이터를 저장(POST)합니다. 
    // 이미 있는 데이터면 업데이트하고 싶을 땐 쿼리 파라미터를 추가해야 하지만, 
    // 일단은 '추가'부터 성공시켜 봅시다!
    call_supabase(url, "POST", j.dump());
}

void update_data_in_db(const stu& s)
{
    // 특정 학번(student_id)만 콕 집어서 수정하라는 URL 쿼리 파라미터 추가
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?student_id=eq." + to_string(s.student_id);

    json j = {
        {"name", s.name},
        {"age", s.age},
        {"gender", s.gender},
        {"baekjoon_id", s.baekjoon_id},
        {"solved_ac_tier", s.solved_ac_tier}
    };

    call_supabase(url, "PATCH", j.dump()); // POST가 아니라 PATCH(수정) 사용
}

void delete_data_from_db(int id)
{
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?student_id=eq." + to_string(id);
    call_supabase(url, "DELETE", ""); // DELETE 메소드 사용
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

    cout << "솔브닥 티어 조회 중...\n";
    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    cout << "조회 완료: " << s.solved_ac_tier << "\n";

    students_map[s.student_id] = s;

    save_data_to_db(s);
}

// 👇 1. 디스코드 전송용으로 문제 추천 결과를 "문자열"로 만들어주는 함수입니다.
string get_recommendation_string(string handle)
{
    string result = "\n**[오늘의 맞춤 추천 문제]**\n";
    string url = "https://solved.ac/api/v3/search/problem?query=solved_by%3A" + handle + "&sort=level&direction=desc";

    try
    {
        auto data = json::parse(call_api(url));
        if (data["count"] == 0)
        {
            return result + "푼 문제가 없어 추천이 불가능합니다.\n";
        }

        int total_solved = data["count"];
        int target_rank = min(total_solved - 1, 10);
        int page = (target_rank / 20) + 1;
        int item_idx = target_rank % 20;

        string paged_url = url + "&page=" + to_string(page);
        auto paged_data = json::parse(call_api(paged_url));

        if (paged_data["items"].size() <= (size_t)item_idx)
        {
            item_idx = (int)paged_data["items"].size() - 1;
        }

        int median_level = paged_data["items"][item_idx]["level"];
        result += "주력 구간: **" + convert_tier(median_level) + "**\n";

        string rec_url = "https://solved.ac/api/v3/search/problem?query=tier%3A" + to_string(median_level) + "%20-solved_by%3A" + handle;
        auto rec_data = json::parse(call_api(rec_url));
        auto& items = rec_data["items"];

        if (items.empty())
        {
            median_level++;
        }

        vector<int> indices(items.size());
        iota(indices.begin(), indices.end(), 0);
        shuffle(indices.begin(), indices.end(), mt19937{ random_device{}() });

        // 디스코드 채팅창 가독성을 위해 5문제만 뽑아서 문자열로 조립합니다.
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

// 👇 2. 조립된 학생 정보와 추천 문제를 디스코드로 쏘는 함수입니다.
void send_discord_profile_and_rec(const stu& s, string rec_str)
{
    // 승훈님의 웹훅 URL
    string webhook_url = "https://discordapp.com/api/webhooks/1491991296242487360/k5tDO-4atCmI3rWn9Bbi_6G-xZwArWlnAqzQSZT0YyJluZsycGURfwMcq7GTE9otCl0D";

    // 디스코드에 보낼 메시지를 마크다운 형식으로 예쁘게 조립합니다.
    string message = "**[O(1) 부원 정보 조회]**\n";
    message += "**이름:** " + s.name + " (" + to_string(s.student_id) + ")\n";
    message += "**티어:** " + s.solved_ac_tier + "\n";
    message += "**백준 프로필:** https://solved.ac/profile/" + s.baekjoon_id + "\n";
    message += rec_str; // 위에서 만든 추천 문제 문자열을 갖다 붙입니다.

    json j = { {"content", message} };
    string post_data = j.dump();

    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

// 👇 3. 기존 검색 함수 개조판 (위의 두 함수를 활용합니다)
void search_students()
{
    int id;
    cout << "조회할 학번: "; cin >> id;
    if (students_map.find(id) != students_map.end())
    {
        stu& s = students_map[id];
        cout << "--- " << s.name << " 학생 정보 ---\n";
        cout << "ID: " << s.student_id << "\n" << "이름: " << s.name << "\n";
        cout << "나이: " << s.age << "\n" << "성별: " << s.gender << "\n";
        cout << "백준: " << s.baekjoon_id << " (" << s.solved_ac_tier << ")\n";

        cout << "\n문제를 추천받으시겠습니까? (1: Yes, 0: No): ";
        int rec; cin >> rec;

        string rec_str = ""; // 추천 문제 결과를 담을 빈 문자열
        if (rec == 1)
        {
            recommend_problems(s.baekjoon_id); // 콘솔에 기존처럼 출력
            rec_str = get_recommendation_string(s.baekjoon_id); // 디스코드 전송을 위해 문자열로도 저장
        }

        // 디스코드 전송 여부 묻기
        cout << "\n이 정보와 추천 문제를 디스코드로 전송하시겠습니까? (1: Yes, 0: No): ";
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

    s.solved_ac_tier = get_tier_from_api(s.baekjoon_id);
    cout << "정보 및 티어가 수정되었습니다.\n";
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
    else
    {
        cout << "찾을 수 없습니다.\n";
    }
}





void filter_students()
{
    cout << "\n--- 필터링 옵션 ---\n";
    cout << "1. 성별로 검색\n";
    cout << "2. 학년으로 검색\n";
    cout << "3. 백준 티어로 검색\n";
    cout << "선택: ";

    int filter_choice;
    cin >> filter_choice;

    if (filter_choice == 1)
    {
        string target_gender;
        cout << "검색할 성별 입력 (예: 남, 여): ";
        cin >> target_gender;

        cout << "\n--- [" << target_gender << "] 학생 목록 ---\n";
        int count = 0;
        for (const auto& item : students_map)
        {
            const stu& s = item.second;
            if (s.gender == target_gender)
            {
                cout << "[" << s.student_id << "] " << s.name
                    << " (" << s.age << "세) - " << s.solved_ac_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else if (filter_choice == 2)
    {
        char target_grade;
        cout << "검색할 학년 입력 (예: 1, 2, 3): ";
        cin >> target_grade;

        cout << "\n--- [" << target_grade << "학년] 학생 목록 ---\n";
        int count = 0;
        for (const auto& item : students_map)
        {
            const stu& s = item.second;
            string id_str = to_string(s.student_id);

            if (id_str[0] == target_grade)
            {
                cout << "[" << s.student_id << "] " << s.name
                    << " (" << s.gender << ") - " << s.solved_ac_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else if (filter_choice == 3)
    {
        string target_tier;
        cout << "검색할 티어 입력 (예: Gold, Silver, Bronze): ";
        cin >> target_tier;

        // 입력 첫 글자를 대문자로 변환 (gold -> Gold) 처리하면 더 완벽합니다.
        target_tier[0] = toupper(target_tier[0]);

        cout << "\n--- [" << target_tier << " 등급] 학생 목록 ---\n";
        int count = 0;
        for (const auto& item : students_map)
        {
            const stu& s = item.second;
            // solved_ac_tier 문자열 안에 target_tier가 포함되어 있는지 확인
            if (s.solved_ac_tier.find(target_tier) != string::npos)
            {
                cout << "[" << s.student_id << "] " << s.name
                    << " (" << s.baekjoon_id << ") - " << s.solved_ac_tier << "\n";
                count++;
            }
        }
        if (count == 0) cout << "해당하는 학생이 없습니다.\n";
    }
    else
    {
        cout << "잘못된 선택입니다. 메뉴로 돌아갑니다.\n";
    }
}

void load_data_from_db()
{
    cout << "\n[클라우드 데이터베이스에서 정보를 불러오는 중...]\n";

    // 1. 모든 컬럼을 가져오기 위한 URL (select=*)
    string url = "https://yxukggorpqsrwurbamxv.supabase.co/rest/v1/students?select=*";

    // 2. GET 방식으로 데이터 요청
    string response = call_supabase(url, "GET");

    if (response.empty() || response == "[]")
    {
        cout << "[알림] DB에 저장된 데이터가 없습니다.\n";
        return;
    }

    try
    {
        // 3. JSON 파싱 (배열 형태로 들어옵니다)
        auto data = json::parse(response);

        // 기존 맵을 비우고 새로 채울지, 합칠지 정할 수 있습니다. 
        // 여기서는 새로 덮어쓰는 방식으로 갈게요.
        students_map.clear();

        for (const auto& item : data)
        {
            stu s;
            // JSON 키 값은 Supabase 테이블의 컬럼명과 일치해야 합니다.
            s.student_id = item["student_id"];
            s.name = item["name"];
            s.age = item["age"];
            s.gender = item["gender"];
            s.baekjoon_id = item["baekjoon_id"];
            s.solved_ac_tier = item["solved_ac_tier"];

            students_map[s.student_id] = s;
        }

        cout << "성공적으로 " << students_map.size() << "명의 데이터를 동기화했습니다!\n";
    }
    catch (const exception& e)
    {
        cout << "데이터 파싱 중 오류 발생: " << e.what() << "\n";
    }
}


void send_discord_webhook(string name, string old_tier, string new_tier)
{
    // 👇 아까 복사한 디스코드 웹훅 URL을 여기에 붙여넣으세요!
    string webhook_url = "https://discordapp.com/api/webhooks/1491991296242487360/k5tDO-4atCmI3rWn9Bbi_6G-xZwArWlnAqzQSZT0YyJluZsycGURfwMcq7GTE9otCl0D";

    // 디스코드에 보낼 메시지를 예쁘게 조립합니다. (마크다운 ** 굵게 적용)
    string message = "🎉 **속보** 🎉\n[" + name + "] 학생이 **" + old_tier + "**에서 **" + new_tier + "**(으)로 승급했습니다! 모두 축하해주세요!";

    json j = {
        {"content", message}
    };
    string post_data = j.dump();

    CURL* curl;
    CURLcode res;
    string readBuffer; // 응답을 받아줄 버퍼

    curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist* headers = NULL;
        // 디스코드는 Supabase와 달리 복잡한 인증키 없이 이 헤더 하나면 끝납니다.
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());

        // 화면에 쓸데없는 디스코드 API 응답이 출력되는 것을 막기 위해 버퍼로 빼줍니다.
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}


void update_all_tiers()
{
    cout << "\n[전체 학생 티어 갱신 시작...]\n";
    int count = 0;

    for (auto& item : students_map)
    {
        stu& s = item.second;
        cout << s.name << "(" << s.baekjoon_id << ") 조회 중... ";

        string new_tier = get_tier_from_api(s.baekjoon_id);

        if (new_tier != "Unknown" && new_tier != "Not Found")
        {
            if (s.solved_ac_tier != new_tier)
            {
                cout << "티어 변동! [" << s.solved_ac_tier << " -> " << new_tier << "]\n";
                string old_tier = s.solved_ac_tier; 
                s.solved_ac_tier = new_tier;

                update_data_in_db(s);
                send_discord_webhook(s.name, old_tier, new_tier);
                count++;
            }
            else
            {
                cout << "변동 없음.\n";
            }
        }
        else
        {
            cout << "조회 실패.\n";
        }
        Sleep(500);
    }

    if (count > 0)
    {
        //save_data();
        cout << "\n총 " << count << "명의 티어가 갱신되었습니다!\n";
    }
    else
    {
        cout << "\n새롭게 변동된 티어가 없습니다.\n";
    }
}


// 👇 1. 점수 계산 함수가 제일 먼저 와야 합니다. (함수 밖으로 빼냄!)
// 👇 1. 점수 계산 함수 (미연동자 꼴찌 처리 완벽 적용!)
int get_tier_score(string tier_str)
{
    // 미연동자는 괘씸죄를 적용하여 -1점(가장 밑바닥)으로 보냅니다.
    if (tier_str == "솔브닥 미연동" || tier_str == "Unknown" || tier_str == "Not Found")
    {
        return -1;
    }
    // 연동은 했지만 아직 문제를 안 푼 사람은 0점
    if (tier_str == "Unrated")
    {
        return 0;
    }
    if (tier_str == "Master")
    {
        return 31; // 마스터는 최고점
    }

    stringstream ss(tier_str);
    string color;
    int rank = 5; // 혹시 숫자를 못 읽을 경우를 대비해 기본값 5(최하위)로 설정

    ss >> color >> rank;

    int base = 0;
    if (color == "Bronze") base = 0;
    else if (color == "Silver") base = 5;
    else if (color == "Gold") base = 10;
    else if (color == "Platinum") base = 15;
    else if (color == "Diamond") base = 20;
    else if (color == "Ruby") base = 25;

    // 백준은 1이 가장 높고 5가 가장 낮으므로, (6 - rank)를 더해줍니다.
    return base + (6 - rank);
}

// 👇 2. 그다음 랭킹 출력 함수가 옵니다.
void print_ranking()
{
    cout << "\n🏆 [O(1) 명예의 전당 - 백준 티어 랭킹] 🏆\n";
    cout << "==========================================\n";

    vector<stu> sorted_students;

    // map의 데이터를 vector로 모두 복사
    for (const auto& item : students_map)
    {
        sorted_students.push_back(item.second);
    }

    // 티어 점수를 기준으로 내림차순(높은 순) 정렬 (C++ 람다 함수 사용)
    sort(sorted_students.begin(), sorted_students.end(), [](const stu& a, const stu& b)
        {
            return get_tier_score(a.solved_ac_tier) > get_tier_score(b.solved_ac_tier);
        });

    // 정렬된 벡터 출력
    int rank = 1;
    for (const auto& s : sorted_students)
    {
        cout << rank << "위: [" << s.name << "] " << s.solved_ac_tier
            << " (학번: " << s.student_id << ", 백준: " << s.baekjoon_id << ")\n";
        rank++;
    }
    cout << "==========================================\n";
}

// 👇 3. 기존의 전체 출력 함수는 깔끔하게 본인 할 일만 하도록 둡니다.
void print_all_students()
{
    cout << "\n--- 전체 학생 목록 ---\n";
    for (const auto& item : students_map)
    {
        const stu& s = item.second;
        cout << "[" << s.student_id << "] " << s.name
            << " (" << s.age << "세, " << s.gender << ") - " << s.solved_ac_tier << "\n";
    }
}


int main()
{
    // 한글 깨짐 방지 및 API 초기화
    system("chcp 65001");
    curl_global_init(CURL_GLOBAL_ALL);

    // 1. 기존 데이터 불러오기
    load_data_from_db();
    //load_data();

    // 2. 시작하자마자 전체 티어 실시간 갱신
    if (!students_map.empty())
    {
        update_all_tiers();
    }
    else
    {
        cout << "[알림] 등록된 학생이 없어 자동 갱신을 건너뜁니다.\n";
    }

    // 3. 메뉴 루프 시작
    while (1)
    {
        // 메뉴판 텍스트 수정: 7.랭킹출력 8.종료
        cout << "\n1.등록 2.검색 3.수정 4.삭제 5.전체출력 6.필터링 7.랭킹출력 8.종료\n선택: ";
        int choice; cin >> choice;

        if (choice == 1) { register_students(); }
        else if (choice == 2) search_students();
        else if (choice == 3) { modify_students(); }
        else if (choice == 4) { delete_students(); }
        else if (choice == 5) { print_all_students(); }
        else if (choice == 6) { filter_students(); }
        else if (choice == 7) { print_ranking(); } // 랭킹 출력 호출!
        else if (choice == 8) break;               // 8번 누르면 종료!
        else cout << "잘못된 입력입니다.\n";
    }

    curl_global_cleanup();
    return 0;
}