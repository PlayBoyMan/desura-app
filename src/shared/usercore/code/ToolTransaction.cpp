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

#include "Common.h"
#include "ToolTransaction.h"

#ifdef NIX
#include "ToolInstallThread.h"
#endif

using namespace UserCore;
using namespace UserCore::Misc;


ToolTransInfo::ToolTransInfo(bool download, gcRefPtr<ToolTransaction> transaction, gcRefPtr<ToolManager> pToolManager)
	: m_bIsDownload(download)
	, m_pTransaction(transaction)
	, m_pToolManager(pToolManager)
{
	gcAssert(m_pTransaction);

	if (!m_pTransaction)
		m_pTransaction = gcRefPtr<ToolTransaction>::create();

	if (!m_bIsDownload)
		return;

	for (auto t : m_pTransaction->getList())
	{
		UserCore::Misc::ToolProgress tp;

		tp.done= 0;
		tp.percent = 0;
		tp.total = 0;

		auto info = pToolManager->findItem(t.toInt64());

		if (info)
			tp.total = info->getDownloadSize();

		m_vProgress.push_back(tp);
	}
}

ToolTransInfo::~ToolTransInfo()
{
}

void ToolTransInfo::removeItem(DesuraId id)
{
	gcTrace("ToolId {0}", id);
	size_t i = getIndex(id);

	if (i == UINT_MAX)
		return;

	if (m_vProgress.size() >= i+1)
		m_vProgress.erase(m_vProgress.begin()+i);

	m_pTransaction->erase(i);

	if (m_uiCompleteCount == m_pTransaction->size())
		m_pTransaction->onCompleteEvent();
}

void ToolTransInfo::onDLComplete(DesuraId id)
{
	gcTrace("ToolId {0}", id);
	size_t i = getIndex(id);

	if (i == UINT_MAX)
		return;

	m_vProgress[i].done = m_vProgress[i].total;
	m_vProgress[i].percent = 100;

	m_uiCompleteCount++;

	if (m_uiCompleteCount == m_pTransaction->size())
		m_pTransaction->onCompleteEvent();
}

void ToolTransInfo::onDLError(DesuraId id, gcException e)
{
	gcTrace("ToolId {0}", id);
	size_t i = getIndex(id);

	if (i == UINT_MAX)
		return;

	m_pTransaction->onErrorEvent(e);
	m_uiCompleteCount++;

	if (m_uiCompleteCount == m_pTransaction->size())
		m_pTransaction->onCompleteEvent();
}

void ToolTransInfo::onDLProgress(DesuraId id, UserCore::Misc::ToolProgress &prog)
{
	size_t i = getIndex(id);

	if (i == UINT_MAX)
		return;

	m_vProgress[i] = prog;

	UserCore::Misc::ToolProgress tp;

	for (size_t x=0; x<m_vProgress.size(); x++)
	{
		tp.done += m_vProgress[x].done;
		tp.percent += m_vProgress[x].percent;
		tp.total += m_vProgress[x].total;
	}

	tp.percent /= m_pTransaction->size();
	m_pTransaction->onProgressEvent(tp);
}

bool ToolTransInfo::isDownload()
{
	return m_bIsDownload;
}

void ToolTransInfo::getIds(std::vector<DesuraId> &idList)
{
	idList = m_pTransaction->getList();
}

ToolStartRes ToolTransInfo::startNextInstall(std::shared_ptr<IPCToolMain> pToolMain, DesuraId &toolId)
{
	gcTrace("ToolId {0}", toolId);

	if (m_uiCompleteCount == m_pTransaction->size())
		return ToolStartRes::NoToolsLeft;

	toolId = m_pTransaction->get(m_uiCompleteCount);
	auto info = m_pToolManager->findItem(toolId.toInt64());

	if (!info)
	{
		Warning("Failed to find tool for tool install with id {0}", toolId.toInt64());
		m_uiCompleteCount++;
		return startNextInstall(pToolMain, toolId);
	}

	if (info->isInstalled())
	{
		onINComplete();
		return startNextInstall(pToolMain, toolId);
	}

	DesuraId id = info->getId();
	m_pTransaction->onStartInstallEvent(id);

	gcException e = pToolMain->installTool(info);

#ifdef NIX
	if (e.getErrId() == ERR_COMPLETED)
	{
		onINComplete();
		return startNextInstall(pToolMain, toolId);
	}
#endif

	if (e.getErrId() != WARN_OK && e.getErrId() != ERR_UNKNOWNERROR)
	{
		onINError(e);
		return ToolStartRes::Failed;
	}
	else
	{
		UserCore::Misc::ToolProgress prog;

		prog.done = m_uiCompleteCount;
		prog.total = m_pTransaction->size();
		prog.percent = prog.done*100/prog.total;

		m_pTransaction->onProgressEvent(prog);
	}

	return ToolStartRes::Success;
}

void ToolTransInfo::startingIPC()
{
	gcTrace("");
	m_pTransaction->onStartIPCEvent();
}

void ToolTransInfo::onINComplete()
{
	gcTrace("");
	m_uiCompleteCount++;
	UserCore::Misc::ToolProgress prog;

	prog.done = m_uiCompleteCount;
	prog.total = m_pTransaction->size();
	prog.percent = prog.done*100/prog.total;

	m_pTransaction->onProgressEvent(prog);

	if (m_uiCompleteCount == m_pTransaction->size())
		m_pTransaction->onCompleteEvent();
}

void ToolTransInfo::onINError(gcException &e)
{
	gcTrace("");
	Warning("Tool install error: {0}\n",e);

	m_uiCompleteCount = m_pTransaction->size();
	m_pTransaction->onErrorEvent(e);
}

void ToolTransInfo::updateTransaction(gcRefPtr<Misc::ToolTransaction> pTransaction)
{
	m_pTransaction->onStartInstallEvent.reset();
	m_pTransaction->onCompleteEvent.reset();
	m_pTransaction->onErrorEvent.reset();
	m_pTransaction->onProgressEvent.reset();
	m_pTransaction->onStartIPCEvent.reset();

	if (!pTransaction)
		return;

	m_pTransaction->onStartInstallEvent = pTransaction->onStartInstallEvent;
	m_pTransaction->onCompleteEvent = pTransaction->onCompleteEvent;
	m_pTransaction->onErrorEvent = pTransaction->onErrorEvent;
	m_pTransaction->onProgressEvent = pTransaction->onProgressEvent;
	m_pTransaction->onStartIPCEvent = pTransaction->onStartIPCEvent;
}

size_t ToolTransInfo::getIndex(DesuraId id)
{
	auto vList = m_pTransaction->getList();
	auto it = std::find(begin(vList), end(vList), id);

	if (it == end(vList))
		return -1;

	return std::distance(begin(vList), it);
}
