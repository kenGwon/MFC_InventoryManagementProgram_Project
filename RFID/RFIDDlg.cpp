
// RFIDDlg.cpp: 구현 파일
//
#include "pch.h"
#include "RFIDDlg.h"

//#define CONSOLE_DEBUG // 콘솔 디버깅이 필요하면 CONSOLE_DEBUG을 define하기

#ifdef CONSOLE_DEBUG
#pragma comment(linker, "/ENTRY:WinMainCRTStartup /subsystem:console") // 빌드하여 실행했을 때, 콘솔도 함께 뜨도록 만들기 위한 명령
#endif

/* <global scope function... non- CRFIDDlg class context>
  desc: MFC 애플리케이션이 돌아가고 있는 OS가 윈도우11 이상인지 아닌지 확인한다.
        이 함수가 필요한 이유는 윈도우10 환경과 윈도우11 환경에서 MFC 애플리케이션이 완벽 호환 되지 않아 
		몇몇 GUI 컴포넌트들의 크기와 좌표값이 의도한 바와 다르게 출력되기 때문이다.
  return: 윈도우11 이상이면 True를 리턴하고, 윈도우11 미만이면 False를 리턴한다.
*/
BOOL Is_Win11_or_Later()
{
	DWORD dwMajor = 0;
	DWORD dwMinor = 0;
	DWORD dwBuildNumber = 0;

	HMODULE hMod;
	RtlGetVersion_FUNC func;

	hMod = LoadLibrary(TEXT("ntdll.dll"));

	if (hMod)
	{
		func = (RtlGetVersion_FUNC)GetProcAddress(hMod, "RtlGetVersion");

		if (func == 0)
		{
		}
		else
		{
			OSVERSIONINFOEX Info;

			ZeroMemory(&Info, sizeof(OSVERSIONINFOEX));

			Info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

			func(&Info);

			dwMajor = Info.dwMajorVersion;
			dwMinor = Info.dwMinorVersion;
			dwBuildNumber = Info.dwBuildNumber;
		}

		FreeLibrary(hMod);
	}

	// dwMajor가 10 이고 빌드번호가 22000 이상이면 윈도우 11 이다
	if (10 == dwMajor && 22000 <= dwBuildNumber)
	{
#ifdef CONSOLE_DEBUG
		printf("윈도우 11입니다!!!");
#endif
		return TRUE;
	}
	else
	{
#ifdef CONSOLE_DEBUG
		printf("윈도우 10입니다...");
#endif
		return FALSE;
	}
}

/*
  desc: 아래 함수는 마이크로소프트사에서 권장하는 메뉴얼 샘플 코드를 변형한 것인데 의도한대로 작동하지 않아 폐기함...
        https://learn.microsoft.com/ko-kr/windows/win32/sysinfo/verifying-the-system-version
*/
#if 0
BOOL Is_Win11_or_Later()
{
    OSVERSIONINFOEX osvi;
    DWORDLONG dwlConditionMask = 0;
    int op = VER_GREATER_EQUAL;

    // Initialize the OSVERSIONINFOEX structure.

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 10;
    osvi.dwBuildNumber = 22000;

    // Initialize the condition mask.

    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, op);

    // Perform the test.

    return VerifyVersionInfo(
        &osvi,
        VER_MAJORVERSION | VER_BUILDNUMBER,
        dwlConditionMask);
}
#endif

/* <global scope function... non- CRFIDDlg class context>
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


// CRFIDDlg 대화 상자 

/*
  desc: CRFIDDlg 클래스-인스턴스 메세지 맵핑
*/
BEGIN_MESSAGE_MAP(CRFIDDlg, CDialogEx)
	ON_WM_QUERYDRAGICON()
	ON_WM_SYSCOMMAND()
	ON_WM_DRAWITEM()
	ON_WM_PAINT()

	ON_BN_CLICKED(IDC_ABOUT, &CRFIDDlg::OnBnClickedAboutDlg)
	ON_BN_CLICKED(IDC_RFID_CONNECTION, &CRFIDDlg::OnBnClickedRfidConnection)
	ON_CBN_SELCHANGE(IDC_DB_SELECT_COMBO, &CRFIDDlg::OnCbnSelchangeDbSelectCombo)
	ON_BN_CLICKED(IDC_READ_ONCE, &CRFIDDlg::OnBnClickReadOnce)
	ON_BN_CLICKED(IDC_READ_CONTINUE, &CRFIDDlg::OnBnClickReadContinue)
	ON_BN_CLICKED(IDC_READ_USER, &CRFIDDlg::OnBnClickedReadUser)
	ON_BN_CLICKED(IDC_USER_UNAUTHORIZE, &CRFIDDlg::OnBnClickedUserUnauthorize)
	ON_MESSAGE(MESSAGE_READ_CARD, &CRFIDDlg::ReadStuffCard) // kenGwon: 사용자정의 메세지 "MESSAGE_READ_CARD"
END_MESSAGE_MAP()

/*
  desc: CRFIDDlg 생성자
*/
CRFIDDlg::CRFIDDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_RFID_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_kenGwon);

	// 플래그 초기화
	m_flagUserAuthority = FALSE;
	m_flagReadContinue = FALSE;
	m_flagDBConnection = FALSE;
	m_flagRFIDConnection = FALSE;
	m_flagReadCardWorkingThread = FALSE;
	m_flagWindows11 = Is_Win11_or_Later();

	// 변수 초기화
	m_strCardUID = _T("");
	m_strStuffName = _T("");
	m_strUserName = _T("");
	m_strUserAuthority = _T("");
	
	m_strCurrentDBName = "";
	past_card_uid = _T("");

	/* 아래 멤버변수는 OnInitDialog()에서 초기화
	m_ctrlDBcomboBox
	m_stuff_image_rect;
	m_user_image_rect;
	m_stuff_image;
	m_user_image;
	*/
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
	DDX_Text(pDX, IDC_EDIT1, m_strCardUID);
	DDX_Text(pDX, IDC_EDIT2, m_strStuffName);
	DDX_Text(pDX, IDC_EDIT3, m_strUserName);
	DDX_Text(pDX, IDC_EDIT4, m_strUserAuthority);
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

	// 콤보박스 선택지를 채운다.
	m_ctrlDBcomboBox.AddString(_T("IDLE"));
	m_ctrlDBcomboBox.AddString(_T("도서관리"));
	m_ctrlDBcomboBox.AddString(_T("음반관리"));
	m_ctrlDBcomboBox.AddString(_T("와인관리"));

	// 물건을 보여주는 Picture Control에 IDLE 상태의 로고 이미지를 출력한다.
	if (m_flagWindows11)
	{
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(110, 145, 345, 195);
	}
	else
	{
		GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(70, 125, 345, 195);
	}
	GetDlgItem(IDC_STUFF_PICTURE)->GetWindowRect(m_stuff_image_rect);
	ScreenToClient(m_stuff_image_rect);
	PrintImage(_T("img\\IDLE_logo.bmp"), m_stuff_image, m_stuff_image_rect);

	// Edit Control에 안내 메세지를 적는다.
	SetDlgItemText(IDC_EDIT1, _T("여기에 카드UID 출력"));
	SetDlgItemText(IDC_EDIT2, _T("여기에 물품이름 출력"));

	// Edit Control의 폰트 사이즈를 키운다.
	CFont font1, font2;
	font1.CreatePointFont(180, _T("고딕"));
	font2.CreatePointFont(120, _T("고딕"));
	GetDlgItem(IDC_EDIT1)->SetFont(&font1);
	GetDlgItem(IDC_EDIT2)->SetFont(&font2);
	GetDlgItem(IDC_RFID_STATUS)->SetFont(&font2);
	GetDlgItem(IDC_CATEGORY_TEXT)->SetFont(&font2);
	font1.Detach();
	font2.Detach();

	// 관리자를 보여주는 Picture Control에 이미지를 출력한다.
	GetDlgItem(IDC_USER_PICTURE)->GetWindowRect(m_user_image_rect);
	ScreenToClient(m_user_image_rect);
	PrintImage(_T("img\\IDLE_user.bmp"), m_user_image, m_user_image_rect);

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

/* <Message Mapped Function>
  desc: 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서 이 함수를 호출합니다.
*/
HCURSOR CRFIDDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

/*
  desc: 프로그램 Title Bar 왼쪽의 아이콘을 눌렀을 때 나오는 메뉴에서 "RFID 정보"를 눌렀을 때의 동작을 정의
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

/* <Message Mapped Function>
  desc: 소유자 그리기(Owner Draw) 옵션을 True로 준 컴포넌트에 한하여 컴포넌트 디자인 재설정
		(재설정된 컴포넌트ID: IDC_RFID_CONNECTION, IDC_READ_CONTINUE, IDC_READ_ONCE)
*/
void CRFIDDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_RFID_CONNECTION)
	{
		if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
		{
			CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

			if (m_flagRFIDConnection)
			{
				pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(144, 238, 144)); // 연초록색
				pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
			}
			else
			{
				pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(225, 225, 225));
				pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
			}

			pDC->SetTextColor(RGB(0, 0, 0));
			pDC->SetBkMode(TRANSPARENT);
			pDC->DrawText(_T("RFID 연결 toggle"), 14, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
	}

	if (nIDCtl == IDC_READ_CONTINUE)
	{
		if (m_flagUserAuthority)
		{
			GetDlgItem(IDC_READ_CONTINUE)->EnableWindow(TRUE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

				pDC->SetTextColor(RGB(0, 0, 0));
				pDC->SetBkMode(TRANSPARENT);

				if (m_flagReadContinue)
				{
					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(236, 230, 204)); // 아이보리색
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->DrawText(_T("계속읽기 실행중"), 8, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
				else
				{
					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(225, 225, 225));
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->DrawText(_T("계속읽기"), 4, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
			}
		}
		else
		{
			GetDlgItem(IDC_READ_CONTINUE)->EnableWindow(FALSE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

				pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(255, 255, 255));
				pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(185, 185, 185), RGB(185, 185, 185));
				pDC->SetTextColor(RGB(185, 185, 185));
				pDC->SetBkMode(TRANSPARENT);
				pDC->DrawText(_T("계속읽기"), 4, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
		}
	}

	if (nIDCtl == IDC_READ_ONCE)
	{
		if (m_flagUserAuthority)
		{
			GetDlgItem(IDC_READ_ONCE)->EnableWindow(TRUE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				if (lpDrawItemStruct->itemAction & ODA_FOCUS)
				{
					CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(236, 230, 204)); // 아이보리색
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->SetTextColor(RGB(0, 0, 0));
					pDC->SetBkMode(TRANSPARENT);
					pDC->DrawText(_T("1회 읽기"), 5, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
				else
				{
					CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(225, 225, 225));
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->SetTextColor(RGB(0, 0, 0));
					pDC->SetBkMode(TRANSPARENT);
					pDC->DrawText(_T("1회 읽기"), 5, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
			}
		}
		else
		{
			GetDlgItem(IDC_READ_ONCE)->EnableWindow(FALSE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

				pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(255, 255, 255));
				pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(185, 185, 185), RGB(185, 185, 185));
				pDC->SetTextColor(RGB(185, 185, 185));
				pDC->SetBkMode(TRANSPARENT);
				pDC->DrawText(_T("1회 읽기"), 5, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
		}
	}

	if (nIDCtl == IDC_USER_UNAUTHORIZE)
	{
		if (m_flagUserAuthority)
		{
			GetDlgItem(IDC_USER_UNAUTHORIZE)->EnableWindow(TRUE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				if (lpDrawItemStruct->itemAction & ODA_FOCUS)
				{
					CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(236, 230, 204)); // 아이보리색
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->SetTextColor(RGB(0, 0, 0));
					pDC->SetBkMode(TRANSPARENT);
					pDC->DrawText(_T("관리자 인증 해제"), 9, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
				else
				{
					CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

					pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(225, 225, 225));
					pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(128, 128, 128), RGB(125, 125, 125));
					pDC->SetTextColor(RGB(0, 0, 0));
					pDC->SetBkMode(TRANSPARENT);
					pDC->DrawText(_T("관리자 인증 해제"), 9, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				}
			}
		}
		else
		{
			GetDlgItem(IDC_USER_UNAUTHORIZE)->EnableWindow(FALSE);

			if (lpDrawItemStruct->itemAction & ODA_DRAWENTIRE || lpDrawItemStruct->itemAction & ODA_FOCUS || lpDrawItemStruct->itemAction & ODA_SELECT)
			{
				CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

				pDC->FillSolidRect(&lpDrawItemStruct->rcItem, RGB(255, 255, 255));
				pDC->Draw3dRect(&lpDrawItemStruct->rcItem, RGB(185, 185, 185), RGB(185, 185, 185));
				pDC->SetTextColor(RGB(185, 185, 185));
				pDC->SetBkMode(TRANSPARENT);
				pDC->DrawText(_T("관리자 인증 해제"), 9, &lpDrawItemStruct->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
		}
	}

	CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

/* <Message Mapped Function>
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

		dc.SetStretchBltMode(COLORONCOLOR); // 이미지를 축소나 확대를 경우 생기는 손실을 보정

		if (!m_stuff_image.IsNull())
		{
			m_stuff_image.Draw(dc, m_stuff_image_rect); // 그림을 Picture Control 크기로 화면에 출력한다.	
		}
		if (!m_user_image.IsNull())
		{
			m_user_image.Draw(dc, m_user_image_rect); // 그림을 Picture Control 크기로 화면에 출력한다.
		}
	}
}

/* <Message Mapped Function>
  desc: 프로그램 정보 다이얼로그를 띄운다.
*/
void CRFIDDlg::OnBnClickedAboutDlg()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/* <Message Mapped Function>
  desc: RFID 연결 여부를 토글할 목적으로 생성된 "RFID연결" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickedRfidConnection()
{
	// "계속읽기"가 실행중이였다면, 해당 기능을 중단
	if (m_flagReadContinue)
	{
		m_flagReadContinue = FALSE;
		m_flagReadCardWorkingThread = FALSE;
		WaitForSingleObject(m_pThread->m_hThread, 5000);
		SetDlgItemText(IDC_READ_CONTINUE, _T("계속읽기"));
	}

	if (m_flagRFIDConnection == FALSE)
	{
		if (OnConnect())
		{
			m_flagRFIDConnection = TRUE;
			PlaySoundW(_T("sound\\DeviceConnect.wav"), NULL, SND_FILENAME | SND_ASYNC);
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
			PlaySoundW(_T("sound\\DeviceDisconnect.wav"), NULL, SND_FILENAME | SND_ASYNC);
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
	// "계속읽기"가 실행중이였다면, 해당 기능을 중단
	if (m_flagReadContinue)
	{
		m_flagReadContinue = FALSE;
		m_flagReadCardWorkingThread = FALSE;
		WaitForSingleObject(m_pThread->m_hThread, 5000);
	}

	DetachDB(m_strCurrentDBName); // 이전 DB의 연결을 끊는다.
	past_card_uid = _T(""); // DB가 바뀌었으므로, past_card_uid값을 초기화해준다.
	m_flagUserAuthority = FALSE; // DB가 바뀌었으므로, 관리자 권한 플래그도 초기화해준다.

	UpdateData(TRUE);
	CString CBox_select;
	m_ctrlDBcomboBox.GetLBText(m_ctrlDBcomboBox.GetCurSel(), CBox_select);

	if (CBox_select == _T("IDLE"))
	{
		// 변수 초기화
		m_strCurrentDBName = "";
		m_strCardUID = _T("");
		m_strStuffName = _T("");
		m_strUserName = _T("");
		m_strUserAuthority = _T("");

		// Print Control에 IDLE 상태의 로고 이미지를 출력한다.
		if (m_flagWindows11)
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(110, 145, 345, 195);
		}
		else
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(70, 125, 345, 195);
		}
		GetDlgItem(IDC_STUFF_PICTURE)->GetWindowRect(m_stuff_image_rect);
		ScreenToClient(m_stuff_image_rect);
		PrintImage(_T("img\\IDLE_logo.bmp"), m_stuff_image, m_stuff_image_rect);

		GetDlgItem(IDC_USER_PICTURE)->GetWindowRect(m_user_image_rect);
		ScreenToClient(m_user_image_rect);
		PrintImage(_T("img\\IDLE_user.bmp"), m_user_image, m_user_image_rect);

		// Edit Control에 안내 메세지를 적는다.
		SetDlgItemText(IDC_EDIT1, _T("여기에 카드UID 출력"));
		SetDlgItemText(IDC_EDIT2, _T("여기에 물품이름 출력"));

		UpdateData(FALSE);
		Invalidate(TRUE);

		PlaySoundW(_T("sound\\ChangeAlert.wav"), NULL, SND_FILENAME | SND_ASYNC);
		return; // 함수 리턴.
	}
	else if (CBox_select == _T("도서관리"))
	{
		m_strCurrentDBName = m_db_list[mfc_book_management];
		if (m_flagWindows11)
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(160, 90, 240, 320);
		}
		else
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(120, 70, 240, 320);
		}
		SetDlgItemText(IDC_TITLE_NAME, _T("Title:"));
	}
	else if (CBox_select == _T("음반관리"))
	{
		m_strCurrentDBName = m_db_list[mfc_record_management];
		if (m_flagWindows11)
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(160, 90, 240, 320);
		}
		else
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(120, 70, 240, 320);
		}
		SetDlgItemText(IDC_TITLE_NAME, _T("Title:"));
	}
	else if (CBox_select == _T("와인관리"))
	{
		m_strCurrentDBName = m_db_list[mfc_wine_management];
		if (m_flagWindows11)
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(160, 90, 240, 320);
		}
		else
		{
			GetDlgItem(IDC_STUFF_PICTURE)->MoveWindow(120, 70, 240, 320);
		}
		SetDlgItemText(IDC_TITLE_NAME, _T("Name:"));
	}
	else
	{
		AfxMessageBox(_T("error occured in ComboBox!!!"));
		return;
	}

	// picture control의 모양을 변경하고 비운다.
	Invalidate(TRUE);
	GetDlgItem(IDC_STUFF_PICTURE)->GetWindowRect(m_stuff_image_rect);
	ScreenToClient(m_stuff_image_rect);
	m_stuff_image.~CImage();

	GetDlgItem(IDC_USER_PICTURE)->GetWindowRect(m_user_image_rect);
	ScreenToClient(m_user_image_rect);
	m_user_image.~CImage();

	// Edit Control을 비운다.
	m_strCardUID = _T("");
	m_strStuffName = _T("");
	m_strUserName = _T("");
	m_strUserAuthority = _T("");
	
	UpdateData(FALSE);
	Invalidate(TRUE);

	PlaySoundW(_T("sound\\ChangeAlert.wav"), NULL, SND_FILENAME | SND_ASYNC);
	AttachDB(m_strCurrentDBName); // 새로운 DB를 연결한다.
}

/* <Message Mapped Function>
  desc: "1회 읽기" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickReadOnce()
{
	// "계속읽기"모드 실행중인 경우 경고문구를 띄우고 함수 종료
	if (m_flagReadContinue == TRUE)
	{
		AfxMessageBox(_T("계속읽기가 실행중입니다!"));
		return;
	}

	if (!m_flagRFIDConnection || !m_flagDBConnection)
	{
		if (!m_flagRFIDConnection && !m_flagDBConnection)
		{
			AfxMessageBox(_T("1. RFID를 연결하십시오.\n2. 관리 카테고리를 선택하십시오."));
		}
		else if (!m_flagRFIDConnection)
		{
			AfxMessageBox(_T("RFID를 연결하십시오."));
		}
		else if (!m_flagDBConnection)
		{
			AfxMessageBox(_T("관리 카테고리를 선택하십시오."));
		}
	}
	else
	{
		ReadStuffCard(NULL, NULL);
		Invalidate(TRUE);
	}
}

/* <Message Mapped Function>
  desc: "계속읽기" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickReadContinue()
{
	if (!m_flagRFIDConnection || !m_flagDBConnection)
	{
		if (!m_flagRFIDConnection && !m_flagDBConnection)
		{
			AfxMessageBox(_T("1. RFID를 연결하십시오.\n2. 관리 카테고리를 선택하십시오."));
		}
		else if (!m_flagRFIDConnection)
		{
			AfxMessageBox(_T("RFID를 연결하십시오."));
		}
		else if (!m_flagDBConnection)
		{
			AfxMessageBox(_T("관리 카테고리를 선택하십시오."));
		}
	}
	else
	{
		// 플래그 처리  
		if (m_flagReadContinue == FALSE) m_flagReadContinue = TRUE;
		else m_flagReadContinue = FALSE;

		// 스레드 처리
		if (m_flagReadContinue == TRUE)
		{
			m_flagReadCardWorkingThread = TRUE;
			m_pThread = AfxBeginThread(ThreadForReading, this);
			SetDlgItemText(IDC_READ_CONTINUE, _T("계속읽기 ing.."));
		}
		else
		{
			m_flagReadCardWorkingThread = FALSE;
			WaitForSingleObject(m_pThread->m_hThread, 5000);
			SetDlgItemText(IDC_READ_CONTINUE, _T("계속읽기"));
		}
	}
}

/* <Message Mapped Function>
  desc: "관리자 카드 읽기" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickedReadUser()
{
	if (!m_flagRFIDConnection || !m_flagDBConnection)
	{
		if (!m_flagRFIDConnection && !m_flagDBConnection)
		{
			AfxMessageBox(_T("1. RFID를 연결하십시오.\n2. 관리 카테고리를 선택하십시오."));
		}
		else if (!m_flagRFIDConnection)
		{
			AfxMessageBox(_T("RFID를 연결하십시오."));
		}
		else if (!m_flagDBConnection)
		{
			AfxMessageBox(_T("관리 카테고리를 선택하십시오."));
		}
	}
	else
	{
		ReadUserCard(NULL, NULL);
		Invalidate(TRUE);
	}
}

/* <Message Mapped Function>
  desc: "인증 해제" 버튼 컴포넌트가 발생시키는 클릭 메세지에 대응하는 함수
*/
void CRFIDDlg::OnBnClickedUserUnauthorize()
{
	// "계속읽기"가 실행중이였다면, 해당 기능을 중단
	if (m_flagReadContinue)
	{
		m_flagReadContinue = FALSE;
		m_flagReadCardWorkingThread = FALSE;
		WaitForSingleObject(m_pThread->m_hThread, 5000);
		SetDlgItemText(IDC_READ_CONTINUE, _T("계속읽기"));
	}

	if (m_flagUserAuthority)
	{
		m_flagUserAuthority = FALSE;
		m_strUserName = _T("");
		m_strUserAuthority = _T("");

		GetDlgItem(IDC_USER_PICTURE)->GetWindowRect(m_user_image_rect);
		ScreenToClient(m_user_image_rect);
		PrintImage(_T("img\\IDLE_user.bmp"), m_user_image, m_user_image_rect);

		UpdateData(FALSE);
		Invalidate(TRUE);

		PlaySoundW(_T("sound\\DeviceDisconnect.wav"), NULL, SND_FILENAME | SND_ASYNC);
		MessageBox(_T("사용자 인증이 해제되었습니다.\n다시 사용하려면 다시 관리자 카드를 읽어주세요."));
	}
	else
	{
		AfxMessageBox(_T("해제할 관리자 인증이 없습니다."));
	}
}

/*
  desc: 클래스 멤버변수 m_flagReadCardWorkingThread의 값을 리턴한다.
  return: TRUE/FALSE
*/
BOOL CRFIDDlg::get_m_flagReadCardWorkingThread()
{
	return this->m_flagReadCardWorkingThread;
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
#ifdef CONSOLE_DEBUG
		printf("FTDI USB To Serial 연결된 개수 : %d\n", usbnumber);
#endif
		if (is_GetSerialNumber(0, readSerialNumber) == IS_OK)
		{
			if (memcmp(changeSerialNumber, readSerialNumber, sizeof(changeSerialNumber) != 0))
			{
				if (is_SetSerialNumber(0, changeSerialNumber) == IS_OK)
				{
#ifdef CONSOLE_DEBUG 
					printf(" USB To Serial Number 를 변경 하였습니다.\n");
					printf(" FTDI SerialNumber :  %s \n", changeSerialNumber);
#endif
				}
			}
			else
			{
#ifdef CONSOLE_DEBUG 
				printf(" FTDI SerialNumber :  %s \n", changeSerialNumber);
#endif
			}
		}
	}

	//열린 포트번호로 연결
	unsigned long portNumber;
	if (is_GetCOMPort_NoConnect(0, &portNumber) == IS_OK)
	{
#ifdef CONSOLE_DEBUG 
		printf("COM Port : %d\n", portNumber);
#endif
	}

	if (is_OpenSerialNumber(&ftHandle, readSerialNumber, 115200) != IS_OK)
	{
#ifdef CONSOLE_DEBUG 
		printf("USB To Serial과 통신 연결 실패\n");
#endif
		Sleep(100);
		return FALSE;
	}
	else {
#ifdef CONSOLE_DEBUG 
		printf("Serial포트 %d와 통신 연결성공!! \n", portNumber);
#endif
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
#ifdef CONSOLE_DEBUG 
		printf("연결을 닫습니다. ");
#endif
		return TRUE;
	}
	else
	{
#ifdef CONSOLE_DEBUG 
		printf("연결 닫기 실패. ");
#endif
		return FALSE;
	}
}

/*
  desc: DB를 연결한다.
  param: 연결할 DB의 name
*/
void CRFIDDlg::AttachDB(string& DBName)
{
	m_flagDBConnection = TRUE;
	mysql_init(&Connect); // Connect는 pre-compiled header에 전역변수로 정의되어 있음.

	if (mysql_real_connect(&Connect, CONNECT_IP, DB_USER, DB_PASSWORD, DBName.c_str(), DB_PORT, NULL, 0))
	{
#ifdef CONSOLE_DEBUG 
		printf("%s DB 연결성공!!!\n", DBName.c_str());
#endif
	}
	else
	{
		AfxMessageBox(_T("DB연결에 실패했습니다.\nDB서버 개방여부 확인하십시오."));
#ifdef CONSOLE_DEBUG 
		printf("%s DB 연결실패...\n", DBName.c_str());
#endif
	}

	mysql_query(&Connect, "SET Names euckr"); // DB 문자 인코딩을 euckr로 셋팅
}

/*
  desc: DB를 연결을 해제한다.
  param: 연결할 DB의 name
*/
void CRFIDDlg::DetachDB(string& DBName)
{
	m_flagDBConnection = FALSE;
	mysql_close(&Connect);
#ifdef CONSOLE_DEBUG 
	printf("%s DB 연결해제...\n", DBName.c_str()); 
#endif
}

/*
  desc: RFID로 읽은 card_uid를 조건으로 설정하여 DB에서 card_uid에 해당하는 title과 img_path를 가져오는 쿼리를 실행한다.
  param1: 쿼리 조건문에 사용될 card_uid 값
  param2: SQL query로 DB에서 얻어온 값을 넣어줄 참조변수 title
  param3: SQL query로 DB에서 얻어온 값을 넣어줄 참조변수 img_path
  return: 쿼리 조회 결과 반환된 row가 0이라면 DB에 정보가 없는 것으로 판단하고 False리턴,
		  쿼리 조회 결과 반횐된 row가 있으면 DB에 정보가 있는 것으로 판단하고 True리턴
*/
BOOL CRFIDDlg::RunStuffQuery(CString card_uid, CString& title, CString& img_path)
{
	string select_columns, table_name;

	if (m_strCurrentDBName == m_db_list[mfc_book_management])
	{
		select_columns = "title, img_path";
		table_name = "book";
	}
	else if (m_strCurrentDBName == m_db_list[mfc_record_management])
	{
		select_columns = "title, img_path";
		table_name = "record";
	}
	else if (m_strCurrentDBName == m_db_list[mfc_wine_management])
	{
		select_columns = "name, img_path";
		table_name = "wine";
	}

	string query;
	query = string("SELECT ") + select_columns + string(" FROM ") + table_name
		+ string(" WHERE card_uid = '") + string(CT2CA(card_uid)) + string("';");

#ifdef CONSOLE_DEBUG 
	cout << query << endl; 
#endif

	mysql_query(&Connect, query.c_str());
	sql_query_result = mysql_store_result(&Connect);

	if (mysql_num_rows(sql_query_result) == 0)
	{
		return FALSE;
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
	return TRUE;
}

/*
  desc: RFID로 읽은 card_uid를 조건으로 설정하여 DB에서 card_uid에 해당하는 name, authority와 img_path를 가져오는 쿼리를 실행한다.
  param1: 쿼리 조건문에 사용될 card_uid 값
  param2: SQL query로 DB에서 얻어온 값을 넣어줄 참조변수 name
  param2: SQL query로 DB에서 얻어온 값을 넣어줄 참조변수 authority
  param3: SQL query로 DB에서 얻어온 값을 넣어줄 참조변수 img_path
  return: 쿼리 조회 결과 반환된 row가 0이라면 DB에 정보가 없는 것으로 판단하고 False리턴,
		  쿼리 조회 결과 반횐된 row가 있으면 DB에 정보가 있는 것으로 판단하고 True리턴
*/
BOOL CRFIDDlg::RunUserQuery(CString card_uid, CString& name, CString& authority, CString& img_path)
{
	string query;
	query = string("SELECT name, authority, img_path FROM user WHERE card_uid = '") + string(CT2CA(card_uid)) + string("';");

#ifdef CONSOLE_DEBUG 
	cout << query << endl;
#endif

	mysql_query(&Connect, query.c_str());
	sql_query_result = mysql_store_result(&Connect);

	if (mysql_num_rows(sql_query_result) == 0)
	{
		return FALSE;
	}
	else
	{
		sql_row = mysql_fetch_row(sql_query_result);
		if (sql_row != nullptr)
		{
			name = sql_row[0];
			authority = sql_row[1];
			img_path = sql_row[2];
		}
	}

	return TRUE;
}

/*
  desc: RFID로 ISO14443A, ISO15693 규격의 카드와 통신하여 UID를 읽는다.
  param: 읽는 카드의 ISO_type(ISO14443A 또는 ISO15693)
  reuturn: 카드의 uid 값
*/
CString CRFIDDlg::ReadCardUID(uint8_t ISO_type)
{
	CString temp, card_uid = _T("");

	if (ISO_type == ISO14443A)
	{
		// ISO14443A 읽기
		if (is_WriteReadCommand(ftHandle, CM1_ISO14443AB, CM2_ISO14443A_ACTIVE + BUZZER_ON,
			writeLength, wirteData, &readLength, readData) == IS_OK)
		{
#ifdef CONSOLE_DEBUG 
			printf("ISO 14443AB UID : ");
#endif
			for (uint16_t i = 0; i < readLength; i++)
			{
				temp.Format(_T("%02x "), readData[i]);
				card_uid += temp;
#ifdef CONSOLE_DEBUG 
				printf("%02x ", readData[i]);
#endif
			}
#ifdef CONSOLE_DEBUG 
			printf("\n");
#endif
		}
	}
	else if (ISO_type == ISO15693)
	{
		// ISO15693 읽기
		if (is_WriteReadCommand(ftHandle, CM1_ISO15693, CM2_ISO15693_ACTIVE + BUZZER_ON,
			writeLength, wirteData, &readLength, readData) == IS_OK)
		{
#ifdef CONSOLE_DEBUG 
			printf("ISO 15693 UID : ");
#endif
			for (uint16_t i = 0; i < readLength; i++)
			{
				temp.Format(_T("%02x "), readData[i]);
				card_uid += temp;
#ifdef CONSOLE_DEBUG 
				printf("%02x ", readData[i]);
#endif
			}
#ifdef CONSOLE_DEBUG 
			printf("\n");
#endif
		}
	}

	return card_uid;
}

/*
  desc: 물건에 대응하는 ISO15693 타입의 카드를 읽어들이고,
		DB 조회에 성공하면 프로그램 GUI 요소들을 해당 DB값에 맞게 설정하여 출력한다.
		만약 DB 조회에 실패하면 경고창을 띄우고 GUI 요소들을 건드리지 않고 바로 함수를 종료한다.
  param1:
  param2:
  return:
*/
LRESULT CRFIDDlg::ReadStuffCard(WPARAM wParam, LPARAM lParam)
{
	CString card_uid, title, img_path;

	// 카드 UID 읽기
	card_uid = ReadCardUID(ISO15693);

	if (past_card_uid != card_uid && card_uid != _T(""))
	{
		m_strCardUID = card_uid;

		if (!RunStuffQuery(card_uid, title, img_path))
		{
			AfxMessageBox(_T("DB에 등록되지 않은 카드입니다."));
			return 1; // 카드uid 조회 결과 DB에 없는 카드임이 밝혀져 바로 함수 종료.
		}
		m_strStuffName = title;
		PrintImage(img_path, m_stuff_image, m_stuff_image_rect);

		UpdateData(FALSE);

		past_card_uid = card_uid;
	}

	return 0;
}

/*
  desc: 사용자에 대응하는 ISO14443A 타입의 카드를 읽어들이고,
		DB 조회에 성공하면 프로그램 GUI 요소들을 해당 DB값에 맞게 설정하여 출력한다.
		만약 DB 조회에 실패하면 경고창을 띄우고 GUI 요소들을 건드리지 않고 바로 함수를 종료한다.
  param1:
  param2:
  return:
*/
LRESULT CRFIDDlg::ReadUserCard(WPARAM wParam, LPARAM lParam)
{
	CString card_uid, name, authority, img_path;

	// 카드 UID 읽기
	card_uid = ReadCardUID(ISO14443A);

	if (card_uid != _T(""))
	{
		if (!RunUserQuery(card_uid, name, authority, img_path))
		{
			AfxMessageBox(_T("DB에 등록되지 않은 카드입니다."));
			return 1; // 카드uid 조회 결과 DB에 없는 카드임이 밝혀져 바로 함수 종료.
		}
		m_strUserName = name;
		m_strUserAuthority = authority;
		PrintImage(img_path, m_user_image, m_user_image_rect);

		if (authority == _T("1"))
		{
			m_flagUserAuthority = TRUE;
			m_strUserAuthority = _T("관리자");
#ifdef CONSOLE_DEBUG 
			printf("접근 권한 open.\n");
#endif
		}
		else
		{
			m_flagUserAuthority = FALSE;
			m_strUserAuthority = _T("일반사원");
#ifdef CONSOLE_DEBUG 
			printf("접근 권한 close.\n");
#endif
		}

		UpdateData(FALSE);
		Invalidate(TRUE);

		PlaySoundW(_T("sound\\DeviceConnect.wav"), NULL, SND_FILENAME | SND_ASYNC);
		MessageBox(_T("사용자 인증에 성공했습니다."));
	}

	return 0;
}

/*
  desc: img_path를 바탕으로 이미지를 로드하여 Picture Control에 출력한다.
  param: 이미지 경로
*/
void CRFIDDlg::PrintImage(CString img_path, CImage& image_instance, CRect& image_rect)
{
	image_instance.~CImage();
	image_instance.Load(img_path);
	InvalidateRect(image_rect, TRUE);
}
