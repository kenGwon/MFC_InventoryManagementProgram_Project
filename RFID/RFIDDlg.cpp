
// RFIDDlg.cpp: 구현 파일
//
#include "RFIDDlg.h"

// DB 연결용 static 전역변수
static MYSQL Connect;
static MYSQL_RES* sql_query_result;
static MYSQL_ROW sql_row;

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

/*
  desc: "계속읽기" 모드를 담당할 작업스레드. 1초마다 한번씩 "1회 읽기" 메세지를 생성한다.
*/
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

////////////////////////
// CRFIDDlg 대화 상자 //
///////////////////////

/*
  desc: CRFIDDlg 클래스-인스턴스 메세지 맵핑
*/
BEGIN_MESSAGE_MAP(CRFIDDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON3, &CRFIDDlg::OnReadOnce)
	ON_BN_CLICKED(IDC_BUTTON4, &CRFIDDlg::OnReadContinue)
	ON_BN_CLICKED(IDC_BUTTON6, &CRFIDDlg::OnBnClickedButton6)
	ON_BN_CLICKED(IDC_RFID_CONNECTION, &CRFIDDlg::OnBnClickedRfidConnection)
	ON_CBN_SELCHANGE(IDC_DB_SELECT_COMBO, &CRFIDDlg::OnCbnSelchangeDbSelectCombo)

	ON_MESSAGE(MESSAGE_READ_CARD, &CRFIDDlg::ReadCard) // kenGwon: 사용자정의 메세지 추가
END_MESSAGE_MAP()

/*
  desc: CRFIDDlg 생성자
*/
CRFIDDlg::CRFIDDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_RFID_DIALOG, pParent)
	, m_strRfid(_T(""))
	, m_strStuffTitle(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_kenGwon);

	// 플래그 초기화
	m_flagReadContinue = FALSE;
	m_flagRFIDConnection = FALSE;
	m_flagReadCardWorkingThread = FALSE;

	m_strCurrentDBName = m_db_name[mfc_test]; // DB name 초기화
	AttachDB(m_strCurrentDBName); // DB 연결
}

/*
  desc: CRFIDDlg 소멸자
*/
CRFIDDlg::~CRFIDDlg()
{
	OnDisconnect(); // RFID 연결 끊기
	DetachDB(m_strCurrentDBName); // DB 연결 끊기
}

/*
  desc: DDX/DDV 지원입니다.
*/
void CRFIDDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_strRfid);
	DDX_Text(pDX, IDC_EDIT2, m_strStuffTitle);
	DDX_Control(pDX, IDC_DB_SELECT_COMBO, m_ctrlDBcomboBox);
}

/*
  desc: 부모클래스의 초기화함수 OnInitDialog()를 자식클래스 CRFIDDlg가 오버라이딩한다.
*/
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

	m_ctrlDBcomboBox.AddString(_T("도서관리"));
	m_ctrlDBcomboBox.AddString(_T("음반관리"));
	m_ctrlDBcomboBox.AddString(_T("와인관리"));
	m_ctrlDBcomboBox.AddString(_T("출입관리"));

	GetDlgItem(IDC_STUFF_PICTURE)->GetWindowRect(m_image_rect); // 그림 출력에 사용하기 위해 Picture Control의 위치를 얻는다.
	ScreenToClient(m_image_rect); // GetWindowRect로 좌표를 얻으면 캡션과 테두리 영역이 포함되기 때문에 해당 영역을 제외시킨다.

	// 시작화면 이미지를 출력한다.
	m_image.Load(_T("img\\logo1.bmp"));
	InvalidateRect(m_image_rect, TRUE);


	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

/*
  desc:
*/
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

/*
  desc: 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면 아래 코드가 필요합니다. 
        문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는 프레임워크에서 이 작업을 자동으로 수행합니다.
*/
void CRFIDDlg::OnPaint()
{
	CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

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

		if (!m_image.IsNull())
		{
			dc.SetStretchBltMode(COLORONCOLOR); // 이미지를 축소나 확대를 경우 생기는 손실을 보정
			m_image.Draw(dc, m_image_rect); // 그림을 Picture Control 크기로 화면에 출력한다.
		}
	}
}

/*
  desc: 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서 이 함수를 호출합니다.
*/
HCURSOR CRFIDDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

/* <Message Mapped Function>
  desc: "1회 읽기" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnReadOnce()
{
	// "계속읽기"모드 실행중인 경우 경고문구를 띄우고 함수 종료
	if (m_flagReadContinue == TRUE)
	{
		AfxMessageBox(_T("계속읽기가 실행중입니다!"));
		return;
	}

	ReadCard(NULL, NULL);
}

/* <Message Mapped Function>
  desc: "계속읽기" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnReadContinue()
{
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

/* <Message Mapped Function>
  desc: (현재까지는) "소리재생" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickedButton6()
{
	//sndPlaySound("sound.wav", SND_ASYNC | SND_NODEFAULT);
	PlaySoundW(_T("sound\\camera.wav"), NULL, SND_FILENAME | SND_ASYNC);
	//_getch();
}

/* <Message Mapped Function>
  desc: RFID 연결 여부를 토글할 목적으로 생성된 "RFID연결" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickedRfidConnection()
{
	if (m_flagRFIDConnection == FALSE)
	{
		if (OnConnect())
		{
			m_flagRFIDConnection = TRUE;
			SetDlgItemText(IDC_RFID_STATUS, _T("RFID status: Connect!!!"));
			SetDlgItemText(IDC_RFID_CONNECTION, _T("RFID 해제"));
			MessageBox(_T("정상적으로 RFID 연결에 성공했습니다."));
		}
		else
		{
			AfxMessageBox(_T("RFID 연결에 실패했습니다!"));
		}
	}
	else
	{
		if (OnDisconnect())
		{
			m_flagRFIDConnection = FALSE;
			SetDlgItemText(IDC_RFID_STATUS, _T("RFID status: Disconnect..."));
			SetDlgItemText(IDC_RFID_CONNECTION, _T("RFID 연결"));
			MessageBox(_T("정상적으로 RFID 연결이 해제되었습니다."));
		}
		else
		{
			AfxMessageBox(_T("RFID 연결해제에 실패했습니다!"));
		}
	}
}

/* <Message Mapped Function>
  desc: 프로그램 카테고리 선택에 관련된 ComboBox 값을 select하면 발생하는 메세지에 대응하는 함수
		1. 콤보박스의 값을 변경하면, 이전 값에 대응하는 DB 연결을 끊고, 새로운 값에 대응하는 DB를 연결한다.
		2. 콤보박스의 값을 변경하면, picture control의 크기를 변경된 DB의 이미지 출력 사이즈에 맞게 조정한다.
*/
void CRFIDDlg::OnCbnSelchangeDbSelectCombo()
{
	DetachDB(m_strCurrentDBName); // 이전 DB의 연결을 끊는다.

	UpdateData(TRUE);
	CString CBox_select;
	m_ctrlDBcomboBox.GetLBText(m_ctrlDBcomboBox.GetCurSel(), CBox_select);

	if (CBox_select == _T("도서관리"))
	{
		m_strCurrentDBName = m_db_name[mfc_book_management];
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(120, 70, 240, 320);
		SetDlgItemText(IDC_TITLE_NAME, _T("Title:"));
	}
	else if (CBox_select == _T("음반관리"))
	{
		m_strCurrentDBName = m_db_name[mfc_record_management];
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(90, 70, 300, 300);
		SetDlgItemText(IDC_TITLE_NAME, _T("Title:"));
	}
	else if (CBox_select == _T("와인관리"))
	{
		m_strCurrentDBName = m_db_name[mfc_wine_management];
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(150, 70, 180, 320);
		SetDlgItemText(IDC_TITLE_NAME, _T("Name:"));
	}
	else if (CBox_select == _T("출입관리"))
	{
		m_strCurrentDBName = m_db_name[mfc_employee_management];
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(80, 70, 320, 180);
		SetDlgItemText(IDC_TITLE_NAME, _T("Name:"));
	}
	else
	{
		printf("error occured!!! in ComboBox\n");
		return;
	}

	// picture control의 크기를 변경하고, 기존의 이미지를 지운다.
	Invalidate(TRUE);
	GetDlgItem(IDC_STUFF_PICTURE)->GetWindowRect(m_image_rect);
	ScreenToClient(m_image_rect);
	m_image.~CImage();

	AttachDB(m_strCurrentDBName); // 새로운 DB를 연결한다.
}

/*
  desc: DB를 연결한다.
  param: 연결할 DB의 name
*/
void CRFIDDlg::AttachDB(string& DBName)
{
	mysql_init(&Connect); // Connect는 pre-compiled header에 전역변수로 정의되어 있음.

	if (mysql_real_connect(&Connect, CONNECT_IP, DB_USER, DB_PASSWORD, DBName.c_str(), DB_PORT, NULL, 0))
		printf("%s DB 연결성공!!!\n", DBName.c_str());
	else
		printf("%s DB 연결실패...\n", DBName.c_str());

	mysql_query(&Connect, "SET Names euckr"); // DB 문자 인코딩을 euckr로 셋팅
}

/*
  desc: DB를 연결을 해제한다.
  param: 연결할 DB의 name
*/
void CRFIDDlg::DetachDB(string& DBName)
{
	printf("%s DB 연결해제...\n", DBName.c_str());
	mysql_close(&Connect);
}

/*
  desc: RFID로 읽은 card_uid를 바탕으로 DB에서 card_uid에 해당하는 title과 img_path를 가져온다.
  param1: card_uid
  param2: SQL로 DB에서 얻어온 값을 넣어줄 참조변수 title
  param3: SQL로 DB에서 얻어온 값을 넣어줄 참조변수 img_path
*/
void CRFIDDlg::RunSQL(CString card_uid, CString& title, CString& img_path)
{
	string select_columns, table_name;

	if (m_strCurrentDBName == m_db_name[mfc_book_management])
	{
		select_columns = "title, img_path";
		table_name = "book";
	}
	else if (m_strCurrentDBName == m_db_name[mfc_record_management])
	{
		select_columns = "title, img_path";
		table_name = "record";
	}
	else if (m_strCurrentDBName == m_db_name[mfc_wine_management])
	{
		select_columns = "name, img_path";
		table_name = "wine";
	}
	else if (m_strCurrentDBName == m_db_name[mfc_employee_management])
	{
		select_columns = "name, img_path";
		table_name = "employee";
	}
	else
	{
		printf("error ocurred!!! in RunSQL\n");
		//return;
	}

	string query;
	query = string("SELECT ") + select_columns + string(" FROM ") + table_name
		+ string(" WHERE card_uid = '") + string(CT2CA(card_uid)) + string("';");

	mysql_query(&Connect, query.c_str());
	sql_query_result = mysql_store_result(&Connect);

	if (sql_query_result == NULL)
	{
		printf("쿼리 조회 실패...\n");
	}
	else
	{
		sql_row = mysql_fetch_row(sql_query_result);
		if (sql_row != nullptr)
		{
			title = sql_row[0];
			img_path = sql_row[1];
		}
	}

	mysql_free_result(sql_query_result);
}

/*
  desc: img_path를 바탕으로 이미지를 로드하여 Picture Control에 출력한다.
  param: 이미지 경로
*/
void CRFIDDlg::PrintImage(CString img_path)
{
	m_image.~CImage();
	m_image.Load(img_path); 
	InvalidateRect(m_image_rect, TRUE);
}

/*
  desc: RFID 연결을 시도한다.
  return: 연결에 성공하면 TRUE, 실패하면 FALSE.
*/
BOOL CRFIDDlg::OnConnect()
{
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
		Sleep(100);

		return FALSE;
	}
	else {
		printf("Serial포트 %d와 통신 연결성공!! \n", portNumber);
		Sleep(100);

		return TRUE;
	}
}

/*
  desc: RFID 연결을 끊는다.
  return: 연결 해제에 성공하면 TRUE, 실패하면 FALSE.
*/
BOOL CRFIDDlg::OnDisconnect()
{
	// 무선파워를 끊어요.
	is_RfOff(ftHandle);

	//USB 포트를 Close
	if (is_Close(ftHandle) == IS_OK)
	{
		printf("연결을 닫습니다. ");
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*
  desc: 클래스 멤버변수 m_flagReadCardWorkingThread의 값을 리턴한다.
  return: TRUE/FALSE
*/
BOOL CRFIDDlg::get_m_flagReadCardWorkingThread()
{
	if (this->m_flagReadCardWorkingThread)
		return TRUE;
	else
		return FALSE;
}

/*
  desc: RFID 프로토콜을 통해 1회 통신을 시도한다.
  param1:
  param2:
  return:
*/
LRESULT CRFIDDlg::ReadCard(WPARAM wParam, LPARAM lParam)
{
	static CString past_card_uid = _T("");
	CString card_uid, title, img_path;

	// 카드 UID 읽기
	card_uid = ReadCardUID();

	if (past_card_uid != card_uid && card_uid != _T(""))
	{
		m_strRfid = card_uid;

		// 카드 UID를 SQL의 WHERE조건으로 활용하여 쿼리를 날린 뒤, 
		// 현 DB에서 카드 UID에 대응하는 title과 img_path 컬럼 값 받아오기
		RunSQL(card_uid, title, img_path);
		m_strStuffTitle = title;
		PrintImage(img_path);

		UpdateData(FALSE);

		past_card_uid = card_uid;
	}

	return 0;
}

/*
  desc: RFID로 ISO15693, ISO14443A 규격의 카드와 통신하여 UID를 읽는다.
  reuturn: 카드의 UID
*/
CString CRFIDDlg::ReadCardUID()
{
	CString temp, card_uid = _T("");

	// ISO15693 읽기
	if (is_WriteReadCommand(ftHandle, CM1_ISO15693, CM2_ISO15693_ACTIVE + BUZZER_ON,
		writeLength, wirteData, &readLength, readData) == IS_OK)
	{
		printf("ISO 15693 UID : ");
		for (uint16_t i = 0; i < readLength; i++)
		{
			printf("%02x ", readData[i]);
			temp.Format(_T("%02x "), readData[i]);
			card_uid += temp;
		}
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
			card_uid += temp;
		}
		printf("\n");
	}

	return card_uid;
}
