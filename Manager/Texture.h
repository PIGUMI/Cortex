#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace std;

/*
* Textureの管理クラス
* テクスチャの読込・Cacheを管理するクラス
*/

class Texture
{
private:
	struct TextureData
	{
		/*
		* テクスチャ本体
		*/
		ComPtr<ID3D12Resource> texture;
		/*
		* SRVヒープ内のインデックス
		*/
		UINT srvIndex = UINT_MAX;
	};

	/*
	* Flush()でマップに登録されるまでの間、転送設定中のテクスチャを保持するための構造体
	*/
	struct PendingTexture
	{
		string key;
		TextureData data;
	};
public:
	static Texture* Get();
	static void Del();
public:
	/**
	 * @brief テクスチャを読込、SRVを作成する
	 * @param filePath 読み込むテクスチャのファイルパス
	 * @return 成功した場合はtrue、失敗した場合はfalse
	 */
	bool Load(const string& filePath);

	/**
	 * @brief 単色1x1のダミーテクスチャを作成し、SRVを登録する
	 *        (Normal/Specularなど、モデル側にテクスチャが無い場合のフォールバック用)
	 * @param key テクスチャマップに登録するキー(ファイルパスの代わりに使う識別子)
	 * @param r,g,b,a ピクセルの色(0〜255)
	 * @return 成功した場合はtrue、失敗した場合はfalse
	 */
	bool LoadDefault(const string& key, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	/**
	 * @brief フォールバック用のダミーテクスチャ(白・黒・フラット法線)をまとめて読み込む
	 * @return 成功した場合はtrue、失敗した場合はfalse
	 */
	bool LoadDefaults();

	/**
	 * @brief Load/LoadDefaultで積んだ転送をまとめてGPUへ送信し、SRVを確定させる
	 *        (GetTexture/GetSRVGpuHandleで未確定のテクスチャが要求された際に内部からも呼ばれる。
	 *        一括読み込みの後に明示的に呼ぶと、転送をCPUで待たずにまとめてコピーできる)
	 * @return 保留中の転送がすべて成功した場合はtrue、ひとつでも失敗した場合はfalse
	 */
	bool Flush();

public:
	static const string DefaultWhiteKey;
	static const string DefaultBlackKey;
	static const string DefaultNormalKey;

	/**
	 * @brief SRVヒープ内のインデックスを取得する
	 * @param filePath テクスチャのファイルパス
	 * @return SRVヒープ内のインデックス
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpuHandle(const string& filePath);

	/**
	 * @brief テクスチャを取得する
	 * @param filePath テクスチャのファイルパス
	 * @return テクスチャ本体のID3D12Resource
	 */
	ID3D12Resource* GetTexture(const string& filePath);
private:
	bool LoadInternal(const string& filePath);

	/**
	 * @brief 指定キーがすでに読込済み、または転送設定済み・待ちかどうか
	 */
	bool IsLoadedOrPending(const string& key) const;

	/**
	 * @brief Defaultヒープにリソースを作成し、Uploadヒープ経由でCopyコマンドリストへ転送命令を記録する
	 *        (BeginCopy/EndCopyAndWaitは呼び出さず、他のバッチ分と合わせてFlush()でまとめて行う)
	 */
	bool BeginUpload(const string& key, const D3D12_RESOURCE_DESC& texDesc, std::vector<D3D12_SUBRESOURCE_DATA>& subResources);

	bool CreateSRV(TextureData& data);
private:
	unordered_map<string, TextureData> m_textureMap;
private:
	bool m_copyBatchOpen = false;							// Copyコマンドリストが記録中かどうか
	vector<ComPtr<ID3D12Resource>> m_pendingUploadBuffers;	// Flushで実行完了を待つまでCPU側で保持しておくUploadバッファ
	vector<PendingTexture> m_pendingTextures;				// Flushでバリア遷移・SRV登録を待つテクスチャ
private:
	Texture() = default;
	~Texture() = default;
private:
	static Texture* instance;
};
