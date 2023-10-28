
// RFIDDlg.cpp: 구현 파일
//
#include "is_d2xx.h"
#include "pch.h"
#include "framework.h"
#include "RFID.h"
#include "RFIDDlg.h"
#include "afxdialogex.h"

// sound 출력용
#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include "mmsystem.h"
#pragma comment(lib,"winmm.lib") // 라이브러리 불러오기

// 디버깅용
#include <iostream>
#include <string>
using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// DB 연결용
MYSQL Connect;
MYSQL_RES* sql_query_result;
MYSQL_ROW sql_row;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//----------------------------------------------------------------------------------------------------------------------------// 

// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------------------------------------------------------// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

UINT ThreadForReading(LPVOID param)
{
	CRFIDDlg* pMain = (CRFIDDlg*)param;

	while (pMain->get_m_flagReadCardWorkingThread())
	{
		Sleep(1000);
		PostMessage(pMain->m_hWnd, MESSAGE_READ_CARD, NULL, NULL);
	}

	return 0;
}

// CRFIDDlg 대화 상자

/*
  desc: 생성자
*/
CRFIDDlg::CRFIDDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_RFID_DIALOG, pParent)
	, m_strRfid(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// DB 연결
	mysql_init(&Connect); // Connect는 pre-compiled header에 전역변수로 정의되어 있음.

	if (mysql_real_connect(&Connect, CONNECT_IP, DB_USER, DB_PASSWORD, DB_NAME, DB_PORT, NULL, 0)) 
		printf("DB 연결성공!!!\n");
	else 
		printf("DB 연결실패...\n");

	mysql_query(&Connect, "SET Names euckr"); // DB 문자 인코딩을 euckr로 셋팅
}

/*
  desc: 소멸자
*/
CRFIDDlg::~CRFIDDlg()
{
	OnDisconnect();

	// DB 끊기
	mysql_close(&Connect); 
}

void CRFIDDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_strRfid);
}

BEGIN_MESSAGE_MAP(CRFIDDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CRFIDDlg::OnConnect)
	ON_BN_CLICKED(IDC_BUTTON3, &CRFIDDlg::OnReadOnce)
	ON_BN_CLICKED(IDC_BUTTON4, &CRFIDDlg::OnReadContinue)
	ON_BN_CLICKED(IDC_BUTTON2, &CRFIDDlg::OnDisconnect)
	ON_BN_CLICKED(IDC_BUTTON5, &CRFIDDlg::OnBnClickedButton5)
	ON_BN_CLICKED(IDC_BUTTON6, &CRFIDDlg::OnBnClickedButton6)

	// kenGwon: 여기에 메세지 넣기
	ON_MESSAGE(MESSAGE_READ_CARD, &CRFIDDlg::ReadCard)
END_MESSAGE_MAP()


// CRFIDDlg 메시지 처리기

BOOL CRFIDDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.


	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	// "계속읽기" 플래그 초기화
	m_flagReadContinue = FALSE;

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CRFIDDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
// 아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
// 프레임워크에서 이 작업을 자동으로 수행합니다.

void CRFIDDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int32_t cxIcon = GetSystemMetrics(SM_CXICON);
		int32_t cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int32_t x = (rect.Width() - cxIcon + 1) / 2;
		int32_t y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CRFIDDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CRFIDDlg::OnConnect()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
		//열린 포트번호 찾기
	if (is_GetDeviceNumber(&usbnumber) == IS_OK)
	{
		printf("FTDI USB To Serial 연결된 개수 : %d\n", usbnumber);
		if (is_GetSerialNumber(0, readSerialNumber) == IS_OK)
		{
			if (memcmp(changeSerialNumber, readSerialNumber, sizeof(changeSerialNumber) != 0))
			{
				if (is_SetSerialNumber(0, changeSerialNumber) == IS_OK)
				{
					printf(" USB To Serial Number 를 변경 하였습니다.\n");
					printf(" FTDI SerialNumber :  %s \n", changeSerialNumber);
				}
			}
			else
				printf(" FTDI SerialNumber :  %s \n", changeSerialNumber);
		}
	}

	//열린 포트번호로 연결
	unsigned long portNumber;
	if (is_GetCOMPort_NoConnect(0, &portNumber) == IS_OK)
	{
		printf("COM Port : %d\n", portNumber);
	}
	if (is_OpenSerialNumber(&ftHandle, readSerialNumber, 115200) != IS_OK)
	{
		printf("USB To Serial과 통신 연결 실패\n");
		//return -1;
	}
	else {
		printf("Serial포트 %d와 통신 연결성공!! \n", portNumber);
	}

	Sleep(100);
}


void CRFIDDlg::OnReadOnce()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	
	// "계속읽기"모드 실행중인 경우 경고문구를 띄우고 함수 종료
	if (m_flagReadContinue == TRUE)
	{
		AfxMessageBox(_T("계속읽기가 실행중입니다!"));
		return;
	}

	ReadCard(NULL, NULL);
}


void CRFIDDlg::OnReadContinue()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.

	// 플래그 처리  
	if (m_flagReadContinue == FALSE) m_flagReadContinue = TRUE;
	else m_flagReadContinue = FALSE;

	// 스레드 처리
	if (m_flagReadContinue == TRUE)
	{
		m_flagReadCardWorkingThread = TRUE;
		m_pThread = AfxBeginThread(ThreadForReading, this);
		SetDlgItemText(IDC_BUTTON4, _T("계속읽기 ing.."));
	}
	else
	{
		m_flagReadCardWorkingThread = FALSE;
		WaitForSingleObject(m_pThread->m_hThread, 5000);
		SetDlgItemText(IDC_BUTTON4, _T("계속읽기"));
	}
}


void CRFIDDlg::OnDisconnect()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	
	// 무선파워를 끊어요.
	is_RfOff(ftHandle);
	//USB 포트를 Close
	if (is_Close(ftHandle) == IS_OK)
	{
		printf("연결을 닫습니다. ");
	}
}


void CRFIDDlg::OnBnClickedButton5()
{
	/////////////////////////////////  이미지 출력  /////////////////////////////////
	CRect m_rect;
	CBitmap m_bitmap;
	//CString Path_img = "img\\null.bmp";
	CString Path_img = _T("img\\logo1.bmp");
	m_bitmap.DeleteObject();
	m_bitmap.Attach((HBITMAP)LoadImage(NULL, Path_img, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
	CDC* pDC = GetDC();
	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	memDC.SelectObject(m_bitmap);
	((CStatic*)GetDlgItem(IDC_STATIC))->GetWindowRect(m_rect);
	ScreenToClient(&m_rect);
	CClientDC   dc(this);

	dc.BitBlt(80, 10, 320, 240, &memDC, 0, 0, SRCCOPY);

	ReleaseDC(pDC);
	DeleteDC(memDC);
}


void CRFIDDlg::OnBnClickedButton6()
{
	//sndPlaySound("sound.wav", SND_ASYNC | SND_NODEFAULT);
	PlaySoundW(_T("sound.wav"), NULL, SND_FILENAME | SND_ASYNC);
	//_getch();
}

/*
  desc: 클래스 멤버변수 m_flagReadCardWorkingThread의 값을 리턴한다.
  return: TRUE/FALSE
*/
BOOL CRFIDDlg::get_m_flagReadCardWorkingThread()
{
	// TODO: 여기에 구현 코드 추가.
	if (this->m_flagReadCardWorkingThread) return TRUE;
	else return FALSE;
}

/*
  desc: RFID 프로토콜을 통해 1회 통신을 시도한다.
  param1:
  param2: 
  return: 
*/
LRESULT CRFIDDlg::ReadCard(WPARAM wParam, LPARAM lParam)
{
	CString temp, temp1 = _T("");

	// ISO15693 읽기
	if (is_WriteReadCommand(ftHandle, CM1_ISO15693, CM2_ISO15693_ACTIVE + BUZZER_ON,
		writeLength, wirteData, &readLength, readData) == IS_OK)
	{
		printf("ISO 15693 UID : ");
		for (uint16_t i = 0; i < readLength; i++)
		{
			printf("%02x ", readData[i]);
			temp.Format(_T("%02x "), readData[i]);
			temp1 += temp;
		}
		m_strRfid = temp1;
		UpdateData(FALSE);

		printf("\n");
	}

	// ISO14443A 읽기
	if (is_WriteReadCommand(ftHandle, CM1_ISO14443AB, CM2_ISO14443A_ACTIVE + BUZZER_ON,
		writeLength, wirteData, &readLength, readData) == IS_OK)
	{
		printf("ISO 14443AB UID : ");
		for (uint16_t i = 0; i < readLength; i++)
		{
			printf("%02x ", readData[i]);
			temp.Format(_T("%02x "), readData[i]);
			temp1 += temp;
		}
		m_strRfid = temp1;
		UpdateData(FALSE);

		printf("\n");






		// db 값 받아오는지 잠시 테스트
		mysql_query(&Connect, "SELECT * FROM test");
		sql_query_result = mysql_store_result(&Connect);
	
		if (sql_query_result == NULL)
		{
			printf("쿼리 조회 실패\n");
		}
		else
		{
			string temp;
			while ((sql_row = mysql_fetch_row(sql_query_result)) != NULL)
			{
				for (uint32_t i = 0; i < mysql_num_fields(sql_query_result); i++)
				{
					temp += sql_row[i];
					temp += " ";
				}

				cout << temp << endl;
			}
		}
	 
		mysql_free_result(sql_query_result);
	}



	return 0;
}
