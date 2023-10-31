#pragma once
#include "afxdialogex.h"
#include "RFIDDlg.h"


// CAddDataDlg 대화 상자

class CAddDataDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CAddDataDlg)

public:
	CAddDataDlg(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CAddDataDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ADD_DATA };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
};
