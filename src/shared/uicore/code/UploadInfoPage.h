/*
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)
Copyright (C) 2014 Bad Juju Games, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.

Contact us at legal@badjuju.com.
*/

#ifndef DESURA_UPLOADINFOPAGE_H
#define DESURA_UPLOADINFOPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "wx_controls/gcControls.h"
#include "BasePage.h"

#include "Managers.h"
#include "usercore/ItemInfoI.h"

#include "wx/wx.h"
#include <wx/filepicker.h>


///////////////////////////////////////////////////////////////////////////


class uploadResumeThread;
class uploadPrepThread;
class fileValidateThread;

typedef struct
{
	const char* path;
	uint32 res;
}fiInfo;

///////////////////////////////////////////////////////////////////////////////
/// Class UploadInfoPage
///////////////////////////////////////////////////////////////////////////////
class UploadInfoPage : public BasePage
{
public:
	UploadInfoPage( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 400,100 ), long style = wxTAB_TRAVERSAL );
	~UploadInfoPage();

	void dispose() override;

	//this is used to resume uploads
	void setInfo(DesuraId id, gcRefPtr<UserCore::Item::ItemInfoI> pItemInfo) override;
	void setInfo_path(DesuraId id, gcRefPtr<UserCore::Item::ItemInfoI> pItemInfo, const char* path);
	void setInfo_key(DesuraId id, gcRefPtr<UserCore::Item::ItemInfoI> pItemInfo, const char* key);

	void run() override {}

protected:
	friend class LanguageTestDialog;

	void showDialog();

	gcStaticText* m_labText;

	wxTextCtrl* m_tbItemFile;
	//wxFilePickerCtrl* m_dpFile;

	gcButton* m_butUpload;
	gcButton* m_butCancel;
	gcButton* m_butFile;

	void initUpload(const char* path, uint64 start);
	void initUpload(const char* path);

	void fileValidation(const char* path);
	void onFileValidate(bool res, const char* path);

	bool validatePath(wxTextCtrl* ctrl, bool type);
	void onButtonClicked( wxCommandEvent& event );
	void onTextChange( wxCommandEvent& event );
	void onResume();

	void resetAllValues();
	bool validateInput();
	void onResumeComplete(const char* path);

	void onResumeCompleteCB(gcString& file);
	void onError(gcException& e);
	void onComplete(gcString& key);
	void onFileValidationComplete(fiInfo& info);


	EventV onResumeEvent;
private:
	gcRefPtr<UserCore::Thread::MCFThreadI> m_pPrepThread;
	gcRefPtr<UserCore::Thread::MCFThreadI> m_pResumeThread;

	gcRefPtr<WebCore::Misc::ResumeUploadInfo> m_pUpInfo;

	fileValidateThread *m_pVFThread;

	bool m_bResume;
	gcString m_szKey;
};

#endif //DESURA_UPLOADINFOPAGE_H
