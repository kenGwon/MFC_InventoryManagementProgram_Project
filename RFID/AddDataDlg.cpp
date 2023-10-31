// AddDataDlg.cpp: 구현 파일
//

#include "pch.h"
#include "RFID.h"
#include "afxdialogex.h"
#include "AddDataDlg.h"


// CAddDataDlg 대화 상자

IMPLEMENT_DYNAMIC(CAddDataDlg, CDialogEx)

CAddDataDlg::CAddDataDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ADD_DATA, pParent)
{

}

CAddDataDlg::~CAddDataDlg()
{
}

void CAddDataDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CAddDataDlg, CDialogEx)
END_MESSAGE_MAP()


// CAddDataDlg 메시지 처리기
