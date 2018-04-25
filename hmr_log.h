#ifndef HMR_LOG_H
#define HMR_LOG_H

extern enum hmr_log_level global_log_level;

extern void hmr_log_impl(const char *file, unsigned line, const char *func,
					unsigned log_level, const char *fmt, ...);


#define hmr_log(level, fmt, ...)\
	do{\
		if(level < HMR_LOG_LEVEL_LAST && level<=global_log_level)\
			hmr_log_impl(__FILE__, __LINE__, __func__,\
							level, fmt, ## __VA_ARGS__);\
	}while(0)



#define	ERROR_LOG(fmt, ...)		hmr_log(HMR_LOG_LEVEL_ERROR, fmt, \
									## __VA_ARGS__)
#define	WARN_LOG(fmt, ...) 		hmr_log(HMR_LOG_LEVEL_WARN, fmt, \
									## __VA_ARGS__)
#define INFO_LOG(fmt, ...)		hmr_log(HMR_LOG_LEVEL_INFO, fmt, \
									## __VA_ARGS__)	
#define DEBUG_LOG(fmt, ...)		hmr_log(HMR_LOG_LEVEL_DEBUG, fmt, \
									## __VA_ARGS__)
#define TRACE_LOG(fmt, ...)		hmr_log(HMR_LOG_LEVEL_TRACE, fmt,\
									## __VA_ARGS__)
#endif