#include <iostream>
#include <map>
#include<string>
#include <fstream>
#include <curl/curl.h> // libcurl 포함
#include <nlohmann/json.hpp> // json 포함

//동아리 애들 대상으로 솔브닥 api 써봐도 될 듯
//아니면 학교 애들 대상으로 성적 산출 프로그램 만들던가
//그게 되겠냐만


using namespace std;

typedef struct  Students
{
	int student_id = 20210;
	int age = 18;
	string name;
	string sex;
	string baekjoon_id;
	string solved_ac_tier;
	
}stu;

map<int, stu> students_map;	

void register_students()
{
	stu s;

	cout << "학생 ID: ";
	cin >> s.student_id;

	cout << "학생 이름: ";
	cin >> s.name;

	cout << "학생 나이: ";
	cin >> s.age;

	cout << "학생 성별: ";
	cin >> s.sex;

	cout << "백준 아이디: ";
	cin >> s.baekjoon_id;

	cout << "백준 솔브닥 티어: ";
	cin >> s.solved_ac_tier;

	students_map[s.student_id] = s;
}

void search_students()
{
	
}

void modify_students()
{
	
}

void delete_students()
{
	
}

void save_data()
{

}

int main()
{
	cout << "1. 학생 등록"<< "\n";
	cout << "2. 학생 검색"<< "\n";
	cout << "3. 학생 정보 수정" << "\n";
	cout << "4. 학생 정보 삭제" << "\n";
	
	int choice;
	cin >> choice;
	
	switch (choice)
	{
		case 1:
		register_students();
		break;
		case 2:
		search_students();
		break;
		case 3:
		modify_students();
		break;
		case 4:
		delete_students();
		break;
	}
	
	return 0;
}