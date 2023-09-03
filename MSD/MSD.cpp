// MSD.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <vector>
#include <iomanip>
#include <Timeapi.h>
#include "Miles.h"
#include "MSD.h"
#include "hooks.h"
#include "Recorder.h"
#include <filesystem>
#include "args.hxx"	

args::ArgumentParser parser("Miles 10 Sound Dumper 作者: Lyxica\n版本号: v1.0-beta5\n修复及汉化更新: zxcPandora");
args::ValueFlag<std::string> audioFolder(parser, "../../r2/sound", "包含以下音频文件的文件夹 (*.mprj, *.mbnk, *.mstr).", { "folder" }, { "../../r2/sound" });
args::ValueFlag<std::string> outputFolder(parser, "/miles_audio", "用于放置导出音频文件的文件夹，默认为软件同级目录的miles_audio文件夹。", { 'o', "out" }, { "./miles_audio" });
args::ValueFlag<std::string> exportLanguage(parser, "Language", "你希望导出的音频语言（详见语言名称.txt）\n如果语言选项不存在或者为空，将默认为所能找到的第一个语言。", { 'e', "export" });
args::Flag listBankEvents(parser, "EVENTLIST", "列出数据库中包含的所有音频名称及ID。", { 'l', "list" });
args::Flag muteSound(parser, "QUIET", "录制音频时不播放声音。", { 'm', "mute" });
args::PositionalList<int> eventIDs(parser, "EVENT IDs", "输入一个或两个音频ID。只输入一个只会录制该ID的音频。输入两个音频ID将录制这两个音频ID之间的每个音频。");
args::Group advancedGroup(parser, "额外功能：");
args::ValueFlag<int> noiseFloor(advancedGroup, "0x2000", "Adjust the noise floor when detecting silence. Any samples below this value will be considered silent.", { "noise" }, 0x2000);
args::ValueFlag<int> beginningSilencePeriod(advancedGroup, "1250", "开始录制音频时，停止之前的等待时间（毫秒）。", { "start" }, 1250);
args::ValueFlag<int> endingSilencePeriod(advancedGroup, "500", "开始录制音频后，停止之前的等待时间（毫秒）。", { "end" }, 500);
args::HelpFlag help(parser, "help", "显示该帮助菜单", { 'h', "help" });
std::vector<int> queuedEvents;
Project project;

int events;
struct {
	int fielda;
	int fieldb;
} out;

Recorder* recorder = 0;
byte* buffer_addr;
int write_size;

__int64 hook_GET_AUDIO_BUFFER_AND_SET_SIZE(__int64* a1, byte** BUFFER, int size) {
	write_size = size;
	auto return_val = hook1(a1, BUFFER, size);
	buffer_addr = *BUFFER;
	return return_val;
}

__int64 hook_TRANSFER_MIXED_AUDIO_TO_SOUND_BUFFER(__int64* a1) {
	if (recorder->Active()) {
		recorder->Append(buffer_addr, write_size);
	}

	return hook2(a1);
}

void WINAPI logM(int number, char* message)
{
	std::cout << "系统日志: " << message << "\r\n";
}

void _Record(Project project) {
	while (queuedEvents.size() > 0) {
		recorder->Record(queuedEvents.back());
		std::cout << "录制 " << recorder->GetName() << " 中" << std::endl;

		MilesBankGetEventTemplateId(project.bank, queuedEvents.back(), (long long*)& out);
		MilesQueueControllerValue(project.queue, "GameMusicVolume", 1);
		MilesQueueControllerValue(project.queue, "DialogueVolume", 1);
		MilesQueueControllerValue(project.queue, "LobbyMusicVolume", 1);
		MilesQueueControllerValue(project.queue, "SfxVolume", 1);
		MilesQueueEventRunByTemplateId(project.queue, (int*)& out);
		SetupBusVolumes(project.driver);
		MilesQueueSubmit(project.queue);

		while (recorder->Active()) {
			Sleep(25);
		}
		queuedEvents.pop_back();
		Sleep(100); // Give the MilesMixer thread time to process any changes.
	}
}
void _Play(Project project) {
	std::string input;
	int i;
	while (true) {
		while (recorder->Active()) {
			if (GetAsyncKeyState(VK_ESCAPE) & 0x80) {
				StopPlaying(project.queue);
				break;
			}
		}
		
		while (true) 
		{
			std::cout << "播放音频ID: ";
			std::cin >> input;
			if (cstrIsDigits(input.c_str()))
			{
				i = atoi(input.c_str());
				break;
			}
			else {
				std::cout << "无效音频ID. 应为0到 " << events << " 之间的数字" << std::endl;
			}
		}

		if (i < 0) {
			StopPlaying(project.queue);
			continue;
		}
		if (i >= events) {
			std::cout << "无效音频ID. 应为0到 " << events << " 之间的数字" << std::endl;
			continue;
		}

		std::cout << "正在播放 " << MilesBankGetEventName(project.bank, i) << " (输入负数来停止播放)" << std::endl;
		MilesBankGetEventTemplateId(project.bank, i, (long long*)& out);
		MilesQueueControllerValue(project.queue, "GameMusicVolume", 1);
		MilesQueueControllerValue(project.queue, "DialogueVolume", 1);
		MilesQueueControllerValue(project.queue, "LobbyMusicVolume", 1);
		MilesQueueControllerValue(project.queue, "SfxVolume", 1);

		MilesQueueEventRunByTemplateId(project.queue, (int*)& out);
		SetupBusVolumes(project.driver);
		MilesQueueSubmit(project.queue);
	}
}
bool cstrIsDigits(const char* string)
{
	int x = 0;
	auto len = strlen(string);

	if (string[0] == '-') { x = 1; }
	while (x < len) {
		if (!isdigit(*(string + x)))
			return false;

		++x;
	}

	return true;
}
int main(int argc, char* argv[])
{	
	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (args::Help)
	{
		std::cout << parser;
		return 0;
	}
	catch (args::ParseError e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	if (!std::filesystem::exists(std::filesystem::path(args::get(audioFolder))))
	{
		std::cout << "目录 " << args::get(audioFolder) << " 未找到" << std::endl;
		return 1;
	}

	project = SetupMiles(&logM, args::get(audioFolder), listBankEvents);
	recorder = new Recorder(project.bank);
	events = MilesBankGetEventCount(project.bank);

	if (listBankEvents) {
		auto names = GetEventNames(project.bank);
		for (const auto& name : names) {
			std::cout << name;
		}

		return 1;
	}

	auto ids = args::get(eventIDs);
	if (ids.size() > 2)
	{
		std::cout << "只能输入两个音频ID" << std::endl;
	}

	if (ids.size() > 0) 
	{
		auto ids = args::get(eventIDs);
		if (ids.size() == 1)
		{
			queuedEvents.push_back(ids[0]);
		}
		else
		{
			for (int i = ids[0]; i <= ids[1]; i++) {
				queuedEvents.push_back(i);
			}
		}
		SetupHooks(project.driver, &hook_GET_AUDIO_BUFFER_AND_SET_SIZE, &hook_TRANSFER_MIXED_AUDIO_TO_SOUND_BUFFER);
		_Record(project);
	}
	else 
	{
		_Play(project);
	}
}
