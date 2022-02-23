// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#import <Cocoa/Cocoa.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <QuartzCore/QuartzCore.h>

@interface BTEMonitor : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
{
    NSMutableArray *peripheralList;
    CBCentralManager *manager;
    CBPeripheral *peripheral;
}

@property (retain) NSMutableArray *peripheralList;

- (void) initialize;
- (void) startScan:(NSString *)InUUID;
- (void) stopScan;
- (BOOL) isLECapableHardware;

@end
