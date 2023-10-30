
// RFIDDlg.h: 헤더 파일
//
#pragma once
#include "afxdialogex.h"
#include "framework.h"
#include "pch.h"
#include "is_d2xx.h"
#include "RFID.h"

// sound 출력
#pragma comment(lib,"winmm.lib")
#include <Windows.h>
#include "mmsystem.h"

// DB 연결
#pragma comment(lib, "libmariadb.lib")
#include "mysql/mysql.h"

// 프로그램 정보 다이얼로그
#include "AboutDlg.h"

// 콘솔 디버깅
#include <conio.h>
#include <stdio.h>
#include <iostream>
using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define CONNECT_IP "127.0.0.1"
#define DB_USER "root"
#define DB_PASSWORD "kgh951220"
#define DB_PORT 3306

#define MESSAGE_READ_CARD WM_USER + 1 // 사용자 정의 메세지

enum DB_Name
{
	mfc_book_management = 0,
	mfc_record_management, 
	mfc_wine_management,
	mfc_employee_management, 
	mfc_test
};

// CRFIDDlg 대화 상자
class CRFIDDlg : public CDialogEx
{
private:
	IS_HANDLE ftHandle = 0;
	char readSerialNumber[100] = "COM07";
	char changeSerialNumber[100] = "RFID01";
	short usbnumber;
	unsigned char wirteData[1024];
	unsigned short writeLength = 0;
	unsigned char readData[1024];
	unsigned short readLength = 0;

	BOOL m_flagReadContinue;
	BOOL m_flagDBConnection;
	BOOL m_flagRFIDConnection;
	BOOL m_flagReadCardWorkingThread;

	string m_db_name[10] = {"mfc_book_management", "mfc_record_management", "mfc_wine_management", "mfc_employee_management", "mfc_test"};
	string m_strCurrentDBName;
	CString m_strRfid;
	CString m_strStuffTitle;

	CWinThread* m_pThread;
	CRect m_image_rect;  // Picture Control의 위치를 기억할 변수
	CImage m_image;  // 사용자가 선택한 이미지 객체를 구성할 변수
	CComboBox m_ctrlDBcomboBox;

protected:
	DECLARE_MESSAGE_MAP()
	HICON m_hIcon;

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnReadOnce();
	afx_msg void OnReadContinue();
	afx_msg void OnBnClickedRfidConnection();
	afx_msg void OnCbnSelchangeDbSelectCombo();
	afx_msg void OnClickedAbout();

public:
	CRFIDDlg(CWnd* pParent = nullptr);
	~CRFIDDlg();
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RFID_DIALOG };
#endif
	void AttachDB(string& DBName);
	void DetachDB(string& DBName);
	void RunSQL(CString card_uid, CString& title, CString& img_path);
	void PrintImage(CString img_path);

	BOOL OnConnect();
	BOOL OnDisconnect();
	BOOL get_m_flagReadCardWorkingThread();

	LRESULT ReadCard(WPARAM wParam, LPARAM lParam);
	CString ReadCardUID();
};
