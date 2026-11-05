#include "FileDialog.h"
#import <AppKit/AppKit.h>

// Cocoa 对话框必须在主线程调用；@autoreleasepool 管理临时 Objective-C 对象。
std::string selectModelFile() {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:@"Open model"];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        if ([panel runModal] != NSModalResponseOK) return {};
        const char* path = [[[panel URL] path] UTF8String];
        return path ? std::string(path) : std::string();
    }
}
