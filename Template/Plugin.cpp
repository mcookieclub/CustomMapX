﻿#include <iostream>
#include <llapi/Global.h>
#include <llapi/EventAPI.h>
#include <llapi/LoggerAPI.h>
#include <llapi/MC/Level.hpp>
#include <llapi/MC/BlockInstance.hpp>
#include <llapi/MC/Block.hpp>
#include <llapi/MC/BlockSource.hpp>
#include <llapi/MC/Actor.hpp>
#include <llapi/MC/Player.hpp>
#include <llapi/MC/ItemStack.hpp>
#include "Version.h"
#include <llapi/LoggerAPI.h>
#include <llapi/LLAPI.h>
#include <llapi/MC/Spawner.hpp>
#include <llapi/ServerAPI.h>
#include <llapi/DynamicCommandAPI.h>
#include <llapi/ScheduleAPI.h>
#include <llapi/MC/MapItem.hpp>
#include <llapi/MC/ServerPlayer.hpp>
#include <llapi/MC/Container.hpp>
#include <llapi/Utils/StringHelper.h>
#include "Setting.h"
std::unordered_map<string, time_t> tempList;

int MapIndex;

Logger logger(PLUGIN_NAME);

inline void getAllFiles(std::string strPath, std::vector<std::string>& vecFiles)
{
	char cEnd = *strPath.rbegin();
	if (cEnd == '\\' || cEnd == '/')
	{
		strPath = strPath.substr(0, strPath.length() - 1);
	}
	if (strPath.empty() || strPath == (".") || strPath == (".."))
		return;
	std::error_code ec;
	std::filesystem::path fsPath(strPath);
	if (!std::filesystem::exists(strPath, ec)) {
		return;
	}
	for (auto& itr : std::filesystem::directory_iterator(fsPath))
	{
		if (std::filesystem::is_directory(itr.status()))
		{
			getAllFiles(UTF82String(itr.path().u8string()), vecFiles);
		}
		else
		{
			vecFiles.push_back(UTF82String(itr.path().filename().u8string()));
		}
	}
}

time_t getTimeStamp()
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	std::time_t timestamp = tmp.count();
	return timestamp;
}
time_t getTimeStamp2()
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
	std::time_t timestamp = tmp.count();
	return timestamp;
}


std::mutex mtx;
#include <llapi/ScheduleAPI.h>
std::tuple<bool, std::vector<unsigned char>, unsigned, unsigned,string> isChange = std::make_tuple(false, std::vector<unsigned char>(), 0, 0,"");


namespace mce{
	
class Blob {
public:
	void* unk0;
	std::unique_ptr<unsigned char[]> buffer;
	size_t length = 0;


	inline Blob() {}
	inline Blob(Blob&& rhs) : buffer(std::move(rhs.buffer)), length(rhs.length) { rhs.length = 0; }
	inline Blob(size_t input_length) : buffer(std::make_unique<unsigned char[]>(input_length)), length(input_length) {}
	inline Blob(unsigned char const* input, size_t input_length) : Blob(input_length) {
		memcpy(buffer.get(), input, input_length);
	}

	inline Blob& operator=(Blob&& rhs) {
		if (&rhs != this) {
			buffer = std::move(rhs.buffer);
			length = rhs.length;
			rhs.length = 0;
		}
		return *this;
	}

	inline Blob clone() const { return { data(), size() }; }

	inline unsigned char* begin() { return buffer.get(); }
	inline unsigned char* end() { return buffer.get() + length; }
	inline unsigned char const* cbegin() const { return buffer.get(); }
	inline unsigned char const* cend() const { return buffer.get() + length; }

	inline unsigned char* data() { return buffer.get(); }
	inline unsigned char const* data() const { return buffer.get(); }

	inline bool empty() const { return length == 0; }
	inline size_t size() const { return length; }

	inline auto getSpan() const { return gsl::make_span(data(), size()); }

};

static_assert(sizeof(Blob) == 24);

enum class ImageFormat {
	NONE = 0,
	RGB = 1,
	RGBA = 2
};

enum class ImageUsage : int8_t {
	unknown = 0,
	sRGB = 1,
	data = 2
};

inline unsigned numChannels(ImageFormat format) {
	switch (format) {
	case ImageFormat::RGB:  return 3;
	case ImageFormat::RGBA: return 4;
	default:                return 0;
	}
}

class Image {
	inline Image(ImageFormat format, unsigned width, unsigned height, ImageUsage usage, Blob&& data)
		: format(format), width(width), height(height), usage(usage), data(std::move(data)) {}

public:
	ImageFormat format{}; // 0x0
	unsigned width{}, height{}; // 0x4, 0x8
	ImageUsage usage{}; // 0xC
	Blob data; // 0x10

	inline Image(Blob&& data) : data(std::move(data)) {}
	inline Image(unsigned width, unsigned height, ImageFormat format, ImageUsage usage)
		: format(format), width(width), height(height), usage(usage) {}
	inline Image() {}

	inline Image& operator=(Image&& rhs) {
		format = rhs.format;
		width = rhs.width;
		height = rhs.height;
		usage = rhs.usage;
		data = std::move(rhs.data);
		return *this;
	}

	inline Image clone() const { return { format, width, height, usage, data.clone() }; }

	inline void copyRawImage(Blob const& blob) { data = blob.clone(); }

	inline bool isEmpty() const { return data.empty(); }

	inline void resizeImageBytesToFitImageDescription() { data = Blob{ width * height * numChannels(format) }; }

	inline void setImageDescription(unsigned width, unsigned height, ImageFormat format, ImageUsage usage) {
		this->width = width;
		this->height = height;
		this->format = format;
		this->usage = usage;
	}

	inline void setRawImage(Blob&& buffer) { data = std::move(buffer); }

	Image(const Image& a1) {
		format = a1.format;
		width = a1.width;
		height = a1.height;
		usage = a1.usage;
		data = a1.data.clone();
	}
};

static_assert(offsetof(Image, data) == 0x10);
static_assert(offsetof(Image, format) == 0x0);
static_assert(offsetof(Image, usage) == 0xC);
static_assert(sizeof(Image) == 40);


}; // namespace mce

class Image2D {
public:
	vector<mce::Color> rawColor;
	
	unsigned width = 0, height = 0;

	Image2D(unsigned w, unsigned h, vector<mce::Color> c) : width(w), height(h), rawColor(c) {};
};
extern HMODULE DllMainPtr;
//#include"MemoryModule.h"
#include "GoLang.hpp"
void golang() {
	//HRSRC DLL = ::FindResource(DllMainPtr, MAKEINTRESOURCE(4), L"DLL");
	//DWORD ResSize = ::SizeofResource(DllMainPtr, DLL);
	//HGLOBAL ResData = ::LoadResource(DllMainPtr, DLL);
	//void* ResDataRef = ::LockResource(ResData);
	//HMEMORYMODULE lib = MemoryLoadLibrary(ResDataRef, ResSize);
	//if (lib) {
	//	GolangFunc::png2PixelArr = (GolangFunc::FuncDef::png2PixelArr)MemoryGetProcAddress(lib, "png2PixelArr");
	//	GolangFunc::getUrlPngData = (GolangFunc::FuncDef::getUrlPngData)MemoryGetProcAddress(lib, "getUrlPngData");
	//}
	//else {
	//	Logger("CustomNpcModule").warn("Failed to load MAP_SubModule");
	//}
	
	if (std::filesystem::exists("plugins/lib/MAP_Golang_Module.dll")) {
		auto lib = LoadLibrary(L"plugins/lib/MAP_Golang_Module.dll");
		if (lib) {
			GolangFunc::png2PixelArr = (GolangFunc::FuncDef::png2PixelArr)GetProcAddress(lib, "png2PixelArr");
			GolangFunc::getUrlPngData = (GolangFunc::FuncDef::getUrlPngData)GetProcAddress(lib, "getUrlPngData");
		}
		else {
			logger.error("Failed to load MAP_SubModule");
		}
	}
	else {
		logger.error("Failed to load MAP_SubModule");
	}
}
namespace Helper {
	vector<char*> split(char* a1) {
		vector<char*> result;
		char* a2 = a1;
		int a3 = 0;
		while (*a2) {
			if (a3 < 2) {
				if (*a2 == '\n') {
					*a2 = 0;
					a3++;
					result.push_back(a1);
					a1 = a2 + 1;
				}
			}
			a2++;
		}
		result.push_back(a1);
		return result;
	}

	const std::tuple<std::vector<unsigned char>, unsigned, unsigned> Png2Pix(string path,ServerPlayer* pl) {
		GoString paths{ path.c_str(),(int64_t)path.size() };
		auto imagedata = GolangFunc::png2PixelArr(paths);
		auto data = imagedata.data();
		if (pl) {
			switch (imagedata.length())
			{
			case -1: {
				pl->sendText("§l§6[CustomMapX] §cImage does not exist!");
				return std::make_tuple(std::vector<unsigned char>(), 0, 0);
			}
			case -2: {
				pl->sendText("§l§6[CustomMapX] §cImage decoding failure!");
				return std::make_tuple(std::vector<unsigned char>(), 0, 0);
			}
			}
		}
		if (imagedata.length() != 0 && data != nullptr) {
			auto out = split(data);
			if (out.size() == 3) {
				int w = atoi(out[0]);
				int h = atoi(out[1]);
				std::vector<unsigned char> image;
				image.resize(w * h * 4);
				memcpy(image.data(), (void*)out[2], w * h * 4);
				if (!pl->isOP())
					tempList[pl->getRealName()] = getTimeStamp();
				return std::make_tuple(image, w, h);
			}
		}
		return std::make_tuple(std::vector<unsigned char>(), 0, 0);
	}

	void Url2Pix(string url,string plname) {
		try {
			GoString urls{ url.c_str(),(int64_t)url.size() };
			auto imagedata = GolangFunc::getUrlPngData(urls);
			auto data = imagedata.data();
			auto pl = Global<Level>->getPlayer(plname);
			if (pl) {
				switch (imagedata.length())
				{
				case -1: {
					pl->sendText("§l§6[CustomMapX] §cURL is not legal!");
					return;
				}
				case -2: {
					pl->sendText("§l§6[CustomMapX] §cImage out of size!");
					return;
				}
				case -3: {
					pl->sendText("§l§6[CustomMapX] §cURL is not a Image!");
					return;
				}
				case -4: {
					pl->sendText("§l§6[CustomMapX] §cURL Access failure!");
					return;
				}
				case -5: {
					pl->sendText("§l§6[CustomMapX] §cImage decoding failure!");
				}
				case -6: {
					pl->sendText("§l§6[CustomMapX] §cImage exceeds maximum allowed size!");
				}
				}
			}
			if (imagedata.length() > 0 && data != nullptr) {
				auto out = split(data);
				if (out.size() == 3) {
					int w = atoi(out[0]);
					int h = atoi(out[1]);
					std::vector<unsigned char> image;
					image.resize(w * h * 4);
					memcpy(image.data(), (void*)out[2], w * h * 4);
					isChange = std::make_tuple(true, image, w, h, plname);
					if(!pl->isOP())
						tempList[plname] = getTimeStamp();
					return;
				}
			}
			isChange = std::make_tuple(false, std::vector<unsigned char>(), 0, 0, "");
			if (pl) {
				pl->sendText("§l§6[CustomMapX] §cAdd Map Error!");
			}
			return;
		}
		catch (...) {
			auto pl = Global<Level>->getPlayer(plname);
			if (pl) {
				isChange = std::make_tuple(false, std::vector<unsigned char>(), 0, 0, "");
				pl->sendText("§l§6[CustomMapX] §cAdd Map Error!");
			}
		}
	}

	vector<Image2D> CuttingImages(std::vector<mce::Color> image, int width, int height) {
		vector<Image2D> images;
		auto wcut = width / 128;
		auto hcut = height / 128;
		if (width % 128 != 0) {
			wcut++;
		}

		if (height % 128 != 0) {
			hcut++;
		}
		for (auto h = 0; h < hcut; h++) {
			for (auto w = 0; w < wcut; w++) {
				vector<mce::Color> img;
				for (auto a = h * 128; a < h * 128 + 128; a++) {
					for (auto b = w * 128; b < w * 128 + 128; b++) {
						if (a * width + b < image.size() && b * height + a < image.size()) {
							img.push_back(image[a * width + b]);
						}
						else {
							img.push_back(mce::Color(0xff, 0xff, 0xff, 0));
						}
					}
				}
				images.push_back(Image2D(128, 128, img));
			}
		}
		return images;
	}
	
	void createImg(std::vector<unsigned char> data, unsigned w, unsigned h, ServerPlayer* sp, string picfile) {
		if (data.size() == 0) return;
		vector<mce::Color> Colorlist;
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				mce::Color img((float)data[(y * w + x) * 4 + 0] / 255, (float)data[(y * w + x) * 4 + 1] / 255, (float)data[(y * w + x) * 4 + 2] / 255, (float)data[(y * w + x) * 4 + 3] / 255);
				Colorlist.push_back(img);
			}
		}
		auto datalist = Helper::CuttingImages(Colorlist, w, h);
		int xtemp = 0;
		int ytemp = 0;
		for (auto data : datalist) {
			auto mapitem = ItemStack::create("minecraft:filled_map");
			//auto MapIndex = sp->getMapIndex();
			//sp->setMapIndex(MapIndex + 1);
			MapIndex++;
			MapItem::setMapNameIndex(*mapitem, MapIndex);
			auto& mapdate = Global<Level>->_createMapSavedData(MapIndex);
			mapdate.setLocked();
			for (int x = 0; x < 128; x++) {
				for (int y = 0; y < 128; y++) {
					mapdate.setPixel(data.rawColor[y + x * 128].toABGR(), y, x);
				}
			}
			mapdate.save(*Global<LevelStorage>);
			MapItem::setItemInstanceInfo(*mapitem, mapdate);
			auto sizetest = sqrt(datalist.size());
			mapitem->setCustomName(picfile + "-" + std::to_string(xtemp) + "_" + std::to_string(ytemp));
			Spawner* sps = &Global<Level>->getSpawner();
			ItemActor* ac = sps->spawnItem(sp->getRegion(), *mapitem, nullptr, sp->getPos(), 0);
			delete mapitem;
			ytemp++;
			if (ytemp == sizetest) {
				xtemp++;
				ytemp = 0;
			}
		}
		sp->sendText("§l§6[CustomMapX] §aAdd Map Success!(" + std::to_string(datalist.size()) + ")");
	}
	
	string rand_str(const int len)
	{
		string str;
		char c;
		int idx;
		for (idx = 0; idx < len; idx++)
		{
			c = 'a' + rand() % 26;
			str.push_back(c);
		}
		return str;
	}
}

void Change() {
	Schedule::repeat([] {
		auto [isChnage, data, w,h,plname] = isChange;
		if (isChnage) {
			isChange = std::make_tuple(false, std::vector<unsigned char>(), 0, 0,"");
			auto sp = Global<Level>->getPlayer(plname);
			if (sp->isPlayer()) {
				Helper::createImg(data, w, h, (ServerPlayer*)sp, Helper::rand_str(5));
			}
		}
		}, 20);
}


void RegCommand()
{
    using ParamType = DynamicCommand::ParameterType;

	vector<string> out;
	getAllFiles(".\\plugins\\CustomMapX\\picture", out);

    auto command = DynamicCommand::createCommand("map", "CustomMapX", CommandPermissionLevel::Any);
	auto& MapReloadEnum = command->setEnum("MapReloadEnum", { "reload" });
	auto& MaphelpEnum = command->setEnum("MaphelpEnum", {"help" });
	auto& MapAddEnum = command->setEnum("MapAddEnum", { "add"});
	auto& MapUrlEnum = command->setEnum("MapUrlEnum", { "download" });
	
    command->mandatory("MapsEnum", ParamType::Enum, MaphelpEnum, CommandParameterOption::EnumAutocompleteExpansion);
	command->mandatory("MapsEnum", ParamType::Enum, MapAddEnum, CommandParameterOption::EnumAutocompleteExpansion);
	command->mandatory("MapsEnum", ParamType::Enum, MapUrlEnum, CommandParameterOption::EnumAutocompleteExpansion);
	command->mandatory("MapsEnum", ParamType::Enum, MapReloadEnum, CommandParameterOption::EnumAutocompleteExpansion);
    command->mandatory("MapSoftEnum", ParamType::SoftEnum, command->setSoftEnum("MapENameList", out));
	command->mandatory("UrlStr", ParamType::String);

    command->addOverload({ MapAddEnum,"MapSoftEnum"});
	command->addOverload({ MaphelpEnum });
	command->addOverload({ MapReloadEnum });
	command->addOverload({ MapUrlEnum, "UrlStr"});
	
	command->setCallback([](DynamicCommand const& command, CommandOrigin const& origin, CommandOutput& output, std::unordered_map<std::string, DynamicCommand::Result>& results) {
		auto action = results["MapsEnum"].get<std::string>();
		string str = "";
		ServerPlayer* sp = origin.getPlayer();
			switch (do_hash(action.c_str()))
			{
			case do_hash("add"): {
				if (sp) {
					if (tempList.find(sp->getRealName()) == tempList.end()) {
						if (sp->isOP() || Settings::LocalImg::allowmember) {
							auto picfile = results["MapSoftEnum"].get<std::string>();
							if (!picfile.empty()) {
								auto [data, w, h] = Helper::Png2Pix(".\\plugins\\CustomMapX\\picture\\" + picfile, sp);
								Helper::createImg(data, w, h, sp, picfile);
							}
						}
						else {
							sp->sendText("§l§6[CustomMapX] §cYou are not allowed to add img map!");
						}
					}
					else {
						sp->sendText("§l§6[CustomMapX] §cFrequent operation, please try again later!\n§gRemaining time:" + std::to_string(Settings::memberRateLimit - ((getTimeStamp() - tempList[sp->getRealName()]) / 1000)) + "s");
					}
				}
				break;
			}
			case do_hash("download"): {
				if (sp) {
					if (tempList.find(sp->getRealName()) == tempList.end()) {
						if (sp->isOP() || Settings::DownloadImg::allowmember) {
							auto url = results["UrlStr"].getRaw<std::string>();
							std::thread th(Helper::Url2Pix, url, sp->getRealName());
							th.detach();
							output.success("§l§6[CustomMapX] §aGenerating, please wait!");
						}
						else {
							sp->sendText("§l§6[CustomMapX] §cYou are not allowed to download img map!");
						}
					}
					else {
						sp->sendText("§l§6[CustomMapX] §cFrequent operation, please try again later!\n§gRemaining time:" + std::to_string(Settings::memberRateLimit - ((getTimeStamp() - tempList[sp->getRealName()]) / 1000)) + "s");
					}
				}
				break;
			}
			case do_hash("reload"): {
				if ((int)origin.getPermissionsLevel() > 0) {
					vector<string> out;
					getAllFiles(".\\plugins\\CustomMapX\\picture", out);
					command.getInstance()->addSoftEnumValues("MapENameList", out);
					Settings::LoadConfigFromJson(JsonFile);
					output.success("§l§6[CustomMapX] §aReload Success!");
				}
				break;
			}
			case do_hash("help"): {
				output.success(
					"§l§e>§6CustomMapX§e<\n"
					"§b/map add §a<mapfile> §gAdd maps\n"
					"§b/map download §a<url> §gDownload maps\n"
					"§b/map reload §gRefresh picture path\n"
					"§b/map help\n"
					"§l§e>§6CustomMapX§e<");
				break;
			}
			default:
				break;
			}
     });
	DynamicCommand::setup(std::move(command));
}

void Sche() {
	Schedule::repeat([] {
		if (tempList.size() > 0) {
			auto nowtime = getTimeStamp();
			for (auto& [name, time] : tempList) {
				if (nowtime - time > Settings::memberRateLimit * 1000) {
					tempList.erase(name);
				}
			}
		}
		}, 20);
}


void loadCfg() {
	//config
	if (!std::filesystem::exists("plugins/CustomMapX"))
		std::filesystem::create_directories("plugins/CustomMapX");
	if (std::filesystem::exists(JsonFile)) {
		try {
			Settings::LoadConfigFromJson(JsonFile);
		}
		catch (std::exception& e) {
			logger.error("Config File isInvalid, Err {}", e.what());
			//Sleep(1000 * 100);
			exit(1);
		}
		catch (...) {
			logger.error("Config File isInvalid");
			//Sleep(1000 * 100);
			exit(1);
		}
	}
	else {
		logger.info("Config with default values created");
		Settings::WriteDefaultConfig(JsonFile);
	}
}


void PluginInit()
{
	srand(NULL);
	MapIndex = getTimeStamp2()/rand()* getTimeStamp2();
	loadCfg();
	golang();
	Logger().info("   ___          _      _____  __ ");
	Logger().info("  / __\\/\\/\\    /_\\    / _ \\ \\/ / ");
	Logger().info(" / /  /    \\  //_\\\\  / /_)/\\  /   \033[38;5;221mVersion:{}", PLUGIN_VERSION_STRING);
	Logger().info("/ /__/ /\\/\\ \\/  _  \\/ ___/ /  \\   \033[38;5;218mGithub:{}", "https://github.com/dreamguxiang/CustomMapX");
	Logger().info("\\____|/    \\/\\_/ \\_/\\/    /_/\\_\\ ");
	Logger().info("");
	if (!std::filesystem::exists("plugins/CustomMapX"))
		std::filesystem::create_directories("plugins/CustomMapX");
	if (!std::filesystem::exists("plugins/CustomMapX/picture"))
		std::filesystem::create_directories("plugins/CustomMapX/picture");
	RegCommand();
	Event::ServerStartedEvent::subscribe([] (const Event::ServerStartedEvent& ev) {
		Change();
		Sche();
		return true;
		});
}

inline vector<string> split(string a1,char a) {
	vector<string> out;
	std::stringstream ss(a1);
	string temp;
	while (getline(ss, temp, a)) {
		out.push_back(temp);
	}
	return out;
}


bool isNextImage1(string name,string newname) {
	
	auto oldoutlist = split(name,'-');
	auto newoutlist = split(newname, '-');
	if (oldoutlist.empty() || newoutlist.empty()) return false;
	if (newname.find(oldoutlist[0]) != newname.npos) {
		auto oldnumlist = split(oldoutlist[1], '_');
		auto newnumlist = split(newoutlist[1], '_');

		auto oldfir = atoi(oldnumlist[0].c_str());
		auto oldsec = atoi(oldnumlist[1].c_str());

		auto newfir = atoi(newnumlist[0].c_str());
		auto newsec = atoi(newnumlist[1].c_str());
		if (oldsec + 1 == newsec) {
			if (oldfir == newfir) {
				return true;
			}
		}
	}
	return false;
}

bool isNextImage2(string name, string newname) {

	auto oldoutlist = split(name, '-');
	auto newoutlist = split(newname, '-');
	if (oldoutlist.empty() || newoutlist.empty()) return false;
	if (newname.find(oldoutlist[0]) != newname.npos) {
		auto oldnumlist = split(oldoutlist[1], '_');
		auto newnumlist = split(newoutlist[1], '_');

		auto oldfir = atoi(oldnumlist[0].c_str());
		auto oldsec = atoi(oldnumlist[1].c_str());

		auto newfir = atoi(newnumlist[0].c_str());
		auto newsec = atoi(newnumlist[1].c_str());
		if(oldfir+1 == newfir) {
			if (newsec == 0) {
				return true;
			}
		}
	}
	return false;
}

bool UseItemSupply(Player* sp, ItemStackBase& item, string itemname, short aux) {
	auto& plinv = sp->getSupplies();
	auto slotnum = dAccess<int, 16>(&plinv);
	auto& uid = sp->getOrCreateUniqueID();
	if (item.getCount() == 0) {
		auto& inv = sp->getInventory();
		bool isgive = 0;
		for (int i = 0; i <= inv.getSize(); i++) {
			auto& item = inv.getItem(i);
			if (!item.isNull()) {
				if (item.getItem()->getSerializedName() == "minecraft:filled_map") {
					if (i == slotnum) continue;
					bool isgive = 0;
					if (isNextImage1(itemname, item.getCustomName())) {
						isgive = 1;
					}
					if (isgive) {
						auto snbt = const_cast<ItemStack*>(&item)->getNbt()->toBinaryNBT();		
						Schedule::delay([snbt, uid, slotnum, i] {
							auto newitem = ItemStack::create(CompoundTag::fromBinaryNBT(snbt));
							auto sp = Global<Level>->getPlayer(uid);
							if (sp) {
								if (sp->getHandSlot()->isNull()) {
									auto& inv = sp->getInventory();
									inv.setItem(i, ItemStack::EMPTY_ITEM);
									auto& plinv = sp->getSupplies();
									inv.setItem(slotnum, *newitem);
									sp->refreshInventory();
								}
							}
							delete newitem;
							}, 1);
					}
					
				}
			}
		}
		Schedule::delay([isgive, uid, slotnum, itemname] {
			auto sp = Global<Level>->getPlayer(uid);
			auto& inv = sp->getInventory();
			if (!isgive) {
				for (int i = 0; i <= inv.getSize(); i++) {
					auto& item = inv.getItem(i);
					if (!item.isNull()) {
						if (item.getItem()->getSerializedName() == "minecraft:filled_map") {

							if (i == slotnum) continue;
							bool isgive2 = 0;
							if (isNextImage2(itemname, item.getCustomName())) {
								isgive2 = 1;
							}
							if (isgive2) {
								auto snbt = const_cast<ItemStack*>(&item)->getNbt()->toBinaryNBT();
								auto& uid = sp->getOrCreateUniqueID();
								Schedule::delay([snbt, uid, slotnum, i] {
								auto newitem = ItemStack::create(CompoundTag::fromBinaryNBT(snbt));
								auto sp = Global<Level>->getPlayer(uid);
								if (sp) {
									if (sp->getHandSlot()->isNull()) {
										auto& inv = sp->getInventory();
										inv.setItem(i, ItemStack::EMPTY_ITEM);
										auto& plinv = sp->getSupplies();
										inv.setItem(slotnum, *newitem);
										sp->refreshInventory();
									}
								}
								delete newitem;
								}, 1);
							}

						}
					}
				}
			}
			},1);
	}
}


TInstanceHook(void, "?useItem@Player@@UEAAXAEAVItemStackBase@@W4ItemUseMethod@@_N@Z", Player, ItemStackBase& item, int a2, bool a3)
{
	auto itemname = item.getCustomName();
	auto itemname2 = item.getItem()->getSerializedName();
	auto aux = item.getAuxValue();
	original(this, item, a2, a3);
	try {
		if (itemname2 == "minecraft:filled_map") {
			UseItemSupply(this, item, itemname, aux);
		}
	}
	catch (...) {
		return;
	}
}

