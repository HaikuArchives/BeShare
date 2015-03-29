#ifndef REMOTE_FILE_ITEM_H
#define REMOTE_FILE_ITEM_H

#include "util/Queue.h"
#include "message/Message.h"

#include <santa/CLVListItem.h>

using namespace muscle;

class ColumnListView;

namespace beshare {

class RemoteUserItem;

// Represents information about a file that is available for download.
// We use objects of this class as keys in our search results hash table.
class RemoteFileItem : public CLVListItem
{
public:
	RemoteFileItem(RemoteUserItem * owner, const char * fileName, const MessageRef & attrs);
	~RemoteFileItem();

	const char * GetFileName() const {return _fileName();}

	virtual void DrawItemColumn(BView * owner, BRect rect, int32 colIdx, bool complete);

	const Message & GetAttributes() const {return *_attributes.GetItemPointer();}

	void Update(BView * view, const BFont * font);

	int Compare(const RemoteFileItem * item2, int32 key) const;

	RemoteUserItem * GetOwner() const {return _owner;}

	const char * GetPath() const;

private:
	RemoteUserItem * _owner;
	String _fileName;
	MessageRef _attributes;
	float _textOffset;
};

};  // end namespace beshare

#endif
