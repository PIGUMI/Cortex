#pragma once
/*
* Componentの基底クラス
* 依存関係を持たないComponentの基底クラス（継承先で依存関係を持つComponentを実装する）
*/

#include <string>

class Component
{
public:
	Component() = default;
	virtual ~Component() = default;
public:
	virtual void Init() = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Release() = 0;
public:
	/**
	 * @brief Componentの名前を取得する
	 * @return Componentの名前
	 */
	std::string GetViewName() const { return m_ViewName; }
	/**
	 * @brief ComponentのIDを取得する
	 * @return ComponentのID
	 */
	int GetID() const { return m_ID; }
	/**
	 * @brief Componentが依存するオブジェクトのポインタを設定する
	 * @param pObject Componentが依存するオブジェクトのポインタ
	*/
	void SetObject(void* pObject) { m_pObject = pObject; }
protected:
	/**
	 * @brief Componentの名前を設定する
	 * @param viewName Componentの名前
	 */
	void SetViewName(const std::string& viewName) { m_ViewName = viewName; }
	/**
	 * @brief ComponentのIDを設定する
	 * @param id ComponentのID
	 */
	void SetID(int id) { m_ID = id; }
protected:
	std::string m_ViewName;// 表示するComponentの名前
	int m_ID; // ComponentのID
	void* m_pObject; // Componentが依存するオブジェクトのポインタ
};

