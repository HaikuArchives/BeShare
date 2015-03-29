#ifndef SHARE_COLUMN_H
#define SHARE_COLUMN_H

#include "util/String.h"

#include <Font.h>
#include <View.h>
#include <santa/CLVColumn.h>

#include "RemoteFileItem.h"
#include "RemoteUserItem.h"

using namespace muscle;

namespace beshare {

class ShareColumn : public CLVColumn
{
public:
	ShareColumn(int type, const char * attrName, const char * label, float width);

	virtual ~ShareColumn() {/* empty */}

	const char * GetAttributeName() const {return _attrName();}

	const char * GetFileCellText(const RemoteFileItem * item) const;
	int Compare(const RemoteFileItem * rf1, const RemoteFileItem * rf2) const;

	enum {
		ATTR_MISC = 0,	// file-specific attrs here
		ATTR_FILENAME,
		ATTR_OWNERNAME,
		ATTR_OWNERID,
		ATTR_OWNERCONNECTION
	};

private:
	mutable char _buf[256];
	int _type;
	String _attrName;
};

};  // end namespace beshare

#endif
