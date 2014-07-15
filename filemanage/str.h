

/* The cJSON structure: */
typedef struct cJSON {
	struct cJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	char *valuestring;			/* The item's string, if type==cJSON_String */
	int valueint;				/* The item's number, if type==cJSON_Number */
	double valuedouble;			/* The item's number, if type==cJSON_Number */

	char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} cJSON;


#pragma pack(1)
typedef struct{
	uint32_t RHeaderSize;
	char     Tid[32];
	void *   RData;
	uint32_t RDataSize;
    uint32_t RStatus;
	void *   ExecCallFunc;
	void *   ResponseSender;
	void *   ResponseStreamSender;
	void *   cJSON_GetObjectItem;
	cJSON *  lpJsonRoot;
	void *   ReadStreamFunc;
	void *   StreamStruct;
}REQUEST_HEADER;
#pragma pack()

int init(char *);
int exec(const char *);