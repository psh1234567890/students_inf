#include <iostream>
#include <map>
#include<string>
#include <fstream>
#include <nlohmann/json.hpp> // json 포함
#include <curl/curl.h> // libcurl 포함

using namespace std;

typedef struct  Students
{
	int student_id;
	int age;
	string name;
	string gender;
	string baekjoon_id;
	string solved_ac_tier;
	//char politic_c;
	
}stu;

map<int, stu> students_map;	

void register_students()
{
	stu s;

	cout << "학생 ID: " << "\n";
	cin >> s.student_id;

	cout << "학생 이름: " << "\n";
	cin >> s.name;

	cout << "학생 나이: " << "\n";
	cin >> s.age;

	cout << "학생 성별: " << "\n";
	cin >> s.gender;

	cout << "백준 아이디: " << "\n";
	cin >> s.baekjoon_id;

	cout << "백준 솔브닥 티어: " << "\n";
	cin >> s.solved_ac_tier;
	
	//cout << "정치 성향(l,r): " << "\n";
	//cin >> s.politic_c;

	students_map[s.student_id] = s;
}

void search_students()
{
	int search_student_id;
	cin >> search_student_id;
	
	if (students_map.empty())
	{
		cout << "등록된 학생이 없습니다." << "\n";
		return;
	}	
	
	if (students_map.find(search_student_id) != students_map.end())
	{
		stu & s = students_map[search_student_id];
		cout << "해당 학번의 학생의 정보를 찾았습니다." << "\n";
		cout << s.name << "\n";
		cout << s.age << "\n";
		cout << s.gender << "\n";
		cout << s.baekjoon_id << "\n";
		cout << s.solved_ac_tier << "\n";
		//cout << students_map[search_student_id].politic_c << "\n";
	}
	else
	{
		cout << "해당 학번의 학생의 정보가 등록되지 않았습니다." << "\n";
		return;
	}
}

void modify_students()
{
	if (students_map.empty())
	{
		cout << "등록된 학생이 없습니다." << "\n";
		return;
	}
	
	int modify_student_id;
	cin >> modify_student_id;
	
	stu s;
	
	cout << "수정할 정보를 차례로 입력하세요." << "\n";
	
	cout << "학생 이름: " << "\n";
	cin >> s.name;

	cout << "학생 나이: " << "\n";
	cin >> s.age;

	cout << "학생 성별: " << "\n";
	cin >> s.gender;

	cout << "백준 아이디: " << "\n";
	cin >> s.baekjoon_id;

	cout << "백준 솔브닥 티어: " << "\n";
	cin >> s.solved_ac_tier;
	
	s.student_id = modify_student_id;
	
	students_map[modify_student_id] = s;
	
	cout << "정보가 수정되었습니다." << "\n";
}

void delete_students()
{
	if (students_map.empty())
	{
		cout << "등록된 학생이 없습니다." << "\n";
		return;
	}
	
	int delete_student_id;
	cout << "삭제할 학생의 학번: " << "\n";
	cin >> delete_student_id;
	
	if (students_map.erase(delete_student_id) == 1)
	{
		cout << "학번 " << delete_student_id << " 학생의 정보가 삭제되었습니다." << "\n";
	}
	else
	{
		cout << "해당 학번을 가진 학생을 찾을 수 없습니다." << "\n";
	}
	
}

void save_data()
{
	ofstream outFile("students.txt");

	if (!outFile.is_open()) {
		cout << "파일을 열 수 없습니다!" << endl;
		return;
	}

	for (const auto & [id, s] : students_map)
	{
		// 데이터 사이에 공백( )을 넣어 구분합니다. 
		// 이름에 띄어쓰기가 있다면 쉼표(,)가 낫지만, 일단 공백으로 가보죠!
		outFile << s.student_id << " "
			<< s.name << " "
			<< s.age << " "
			<< s.gender << " "
			<< s.baekjoon_id << " "
			<< s.solved_ac_tier << "\n";
	}

	outFile.close();
	cout << "모든 데이터가 students.txt에 저장되었습니다." << "\n";
}

void load_data()
{

}
int main()
{
	while (1)
	{
		cout << "1. 학생 등록"<< "\n";
		cout << "2. 학생 검색"<< "\n";
		cout << "3. 학생 정보 수정" << "\n";
		cout << "4. 학생 정보 삭제" << "\n";
		cout << "5. 프로그램 종료" << "\n";
	
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
			
		case 5:
			return 0;
		}
	}
	
	return 0;
}