package com.theotherpancreas.pump;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.os.Looper;
import androidx.annotation.NonNull;

import com.theotherpancreas.cgm.bluetooth.AuthenticationHashTool;
import com.theotherpancreas.cgm.bluetooth.BluetoothConnectionTracker;
import com.theotherpancreas.cgm.bluetooth.DeviceType;
import com.theotherpancreas.cgm.bluetooth.GattPipe;
import com.theotherpancreas.cgm.bluetooth.OverlappingScanner;
import com.theotherpancreas.cgm.bluetooth.OverlappingScannerResult;
import com.theotherpancreas.cgm.bluetooth.messages.CRC;
import com.theotherpancreas.cgm.scheduling.ScheduledScanReceiver;
import com.theotherpancreas.data.LogEntry;
import com.theotherpancreas.pump.basal.ActiveBasalModel;
import com.theotherpancreas.ui.settings.SuccessListener;
import com.theotherpancreas.util.App;
import com.theotherpancreas.util.DefaultExceptionHandler;
import com.theotherpancreas.util.Settings;
import com.theotherpancreas.util.Tools;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Semaphore;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import timber.log.Timber;

/**
 * Created by Mike on 5/9/2016.
 */
public class PumpBluetooth extends BluetoothGattCallback {
    public static final int FLAG_UNSAFE = 0b00000001;
    private static PumpBluetooth instance;

    private GattPipe gattPipe;
    private UUID customService = UUID.fromString("dd43aac7-4ea3-498b-a572-ca33a06707c7");
    private UUID notifyUUID = UUID.fromString("dd43aac8-4ea3-498b-a572-ca33a06707c7");
    private UUID writeUUID = UUID.fromString("dd43aac9-4ea3-498b-a572-ca33a06707c7");
    private UUID readUUID = UUID.fromString("dd43aaca-4ea3-498b-a572-ca33a06707c7");
    private SecureRandom secureRandom = new SecureRandom();

    private Semaphore semaphore = new Semaphore(1, true);
    private ExecutorService threadPool = Executors.newCachedThreadPool(new ThreadFactory() {
        private int id = 0;
        @Override
        public Thread newThread(@NonNull Runnable r) {
            final Thread thread = new Thread(r, "PumpBluetoothThreadPool_" + id++);
            final Thread.UncaughtExceptionHandler uncaughtExceptionHandler = thread.getUncaughtExceptionHandler();

            thread.setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
                @Override
                public void uncaughtException(Thread t, Throwable e) {
                    Timber.e(e, "This thread (%s) had an uncaught exception", thread.getName());
                    uncaughtExceptionHandler.uncaughtException(t, e);
                }
            });
            return thread;
        }
    });


    private boolean pumpNeedsSpeedSetting = false;

    private BluetoothDevice device;
    final OverlappingScanner scanner = new OverlappingScanner();

    public synchronized static PumpBluetooth getInstance() {
        if (instance == null)
            instance = new PumpBluetooth();
        return instance;
    }

    private PumpBluetooth() {
    }


//    public void connect() {
//        new Thread(new Runnable() {
//            @Override
//            public void run() {
//                for (long falloff = 3000; true; falloff *= 2) {
//                    if(requestAck())
//                        break;
//                    try {
//                        Thread.sleep(falloff);
//                    } catch (InterruptedException e) {
//                        Timber.e(e);
//                    }
//
//                }
//            }
//        }).start();
//    }

    public boolean isConnected() {
        return gattPipe != null && device != null && gattPipe.hasDiscoveredServices() && GattPipe.isDeviceConnected(App.context, device.getName()) && requestAck();
    }

    /**
     * returns immediately if the connection is valid
     */
    /*package*/ synchronized boolean blessConnection() throws PumpNotConnectedException {
        if (isConnected()) {
            return true;
        }
        else {
            GattPipe.closeAll(DeviceType.PUMP);
        }
        semaphore.drainPermits();
        createGattPipe(false);
        findPump();

        try {
            Timber.d("Waiting for semaphore to be released");
            boolean connectedInTime = semaphore.tryAcquire(15, TimeUnit.SECONDS);
            if (!connectedInTime)
                throw new PumpNotConnectedException();
            else
                return true;
        } catch (InterruptedException e) {
            DefaultExceptionHandler.appendException(e);
            Timber.e(e);
        }
        return false;
    }

    /*package*/ void findPump() {
        if (Settings.getPumpKey() == null || Settings.getPumpKey().isEmpty())
            return;


        String keyCrcAdvert = CRC.calculateAsString(Settings.getPumpKey().getBytes());
        String setupAdvert = CRC.calculateAsString(Settings.getDefaultPumpKey().getBytes());
        LinkedList<String> advertisingFilter = new LinkedList<>();
        advertisingFilter.add(keyCrcAdvert);
//        if (!keyCrcAdvert.equals(setupAdvert))
//            advertisingFilter.add(setupAdvert);

        Timber.d("CRC %s", keyCrcAdvert);
        BluetoothConnectionTracker.setState(DeviceType.PUMP, BluetoothConnectionTracker.ConnectionState.SCANNING);
        scanner.findDevice("TOPancreas", advertisingFilter, 10, TimeUnit.SECONDS, new OverlappingScannerResult() {
            AtomicBoolean oneShot = new AtomicBoolean(true);
            @Override
            public void onResult(ScanResult result) {
                BluetoothConnectionTracker.setState(DeviceType.PUMP, BluetoothConnectionTracker.ConnectionState.CONNECTING);
                byte[] manufacturerSpecificData = null;

                Timber.v("Here is the list of manufacturer specific data");
                for (int i = 0; i < 256; i++) {

                    manufacturerSpecificData = result.getScanRecord().getManufacturerSpecificData(i);
                    if (manufacturerSpecificData != null)
                        Timber.v("MANU[%s] %s", i, Tools.bytesToHexString(manufacturerSpecificData));
                }


                setDevice(result.getDevice());

                if(getDevice().getBondState() != BluetoothDevice.BOND_BONDED) {
                    boolean bondAttempt = getDevice().createBond();
                    Timber.d("BOND %s", bondAttempt ? "attempted" : "not attempted");
                }
                if (oneShot.getAndSet(false))
                    createGattPipe(true);
//                gattPipe.setCloseGattOnDisconnect(false);
            }

            @Override
            public void onTimeOut()
            {
                Timber.w("Didn't find the pump");
//                if (gattPipe != null)
//                    gattPipe.disconnect();
                BluetoothConnectionTracker.setState(DeviceType.PUMP, BluetoothConnectionTracker.ConnectionState.NONE);
            }

            @Override
            public void onStop() {
                Timber.d("Found the pump, so we'll stop the scan");
            }
        });
    }

    private static int count = 0;

    public void createGattPipe(final boolean directConnection) {

        Tools.runOnNewThread(new Runnable() {
            @Override
            public void run() {
                gattPipe = new GattPipe(DeviceType.PUMP) {
                    @Override
                    protected void onConnectionEstablished() {
                        BluetoothConnectionTracker.setState(DeviceType.PUMP, BluetoothConnectionTracker.ConnectionState.CONNECTED);
                        Timber.d("ConnectionEstablished()");
                        scanner.stopScansBecauseWeConnected();
                        semaphore.release();
                    }

                    @Override
                    protected void onDeviceConnect() {
                        Timber.d("onDeviceConnect()");
                    }
                };

                BluetoothDevice device = getDevice();
                if (device != null) {
                    Timber.d("times here: " + count++);
                    BluetoothConnectionTracker.setState(DeviceType.PUMP, BluetoothConnectionTracker.ConnectionState.CONNECTING);
                    gattPipe.initialize(App.context, device, directConnection);
                }
            }
        });
    }

    public boolean requestAck() {
        Timber.d("Requesting Ack..........................................");
        byte[] result = null;
        try {
            result = transmitUnencrypted((byte) 0x0, new byte[0], (byte) 0x0, 4, TimeUnit.SECONDS);
        } catch (IOException e) {
            Timber.e(e);
        }
        return result != null;
    }

    public synchronized  byte[] transmitUnencrypted(byte opcode, byte[] data, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit) throws IOException {

        try {
            if (Thread.currentThread().equals(Looper.getMainLooper().getThread())) {
                Timber.w("You are attempting to transmit using the MainLooper thread. This is likely to fail");
            }

            //prepend opcode
            byte[] payload = new byte[data.length + 1];
            payload[0] = opcode;
            System.arraycopy(data, 0, payload, 1, data.length);

            BluetoothGattCharacteristic write = gattPipe.getCharacteristic(writeUUID);
            BluetoothGattCharacteristic notify = gattPipe.getCharacteristic(notifyUUID);

            boolean notifySuccess = gattPipe.setCharacteristicNotification(notify, true);
            Timber.v("notifySuccess: %s", notifySuccess);

            BluetoothGattCharacteristic result = gattPipe.writeCharacteristic(write, payload, timeToWaitForOpcode, waitTimeUnit, waitForOpcodeResponse);
            return result == null ? null : result.getValue();
        } catch (Exception e) {
            Timber.e(e);
            throw e;
        }
    }

    public synchronized  byte[] transmitDataSecurely(byte[] data, byte[] salt, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit) throws IOException, PumpNotConnectedException {
        byte opcode = data[0];
        data = Arrays.copyOfRange(data, 1, data.length);
        return transmitDataSecurely(opcode, data, salt, waitForOpcodeResponse, timeToWaitForOpcode, waitTimeUnit);
    }

    public synchronized  byte[] transmitDataSecurely(byte[] data, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit) throws IOException, PumpNotConnectedException {
        byte opcode = data[0];
        data = Arrays.copyOfRange(data, 1, data.length);
        return transmitDataSecurely(opcode, data, waitForOpcodeResponse, timeToWaitForOpcode, waitTimeUnit);
    }

    public synchronized  byte[] transmitDataSecurely(byte opcode, byte[] data, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit) throws IOException, PumpNotConnectedException {
        return transmitDataSecurely(opcode, data, waitForOpcodeResponse, timeToWaitForOpcode, waitTimeUnit, false);
    }


    /**
     * This method requests the salt and then encodes the salt and the provided 0 padded data
     * The data is sent to the pump, and the result with opcode waitForOpcodeResponse is CRC verified,
     * then decoded and the packets data is returned without the opcode and CRC
     * @param data
     * @return
     */

    public synchronized  byte[] transmitDataSecurely(byte opcode, byte[] data, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit, boolean bypassBasalSync) throws IOException, PumpNotConnectedException {
        if (Thread.currentThread().equals(Looper.getMainLooper().getThread())) {
            Timber.w("You are attempting to transmit using the MainLooper thread. This is likely to fail.");
        }
        byte[] saltPayload = requestSalt();

        byte[] salt = Arrays.copyOfRange(saltPayload, 1, 5);
        if (!bypassBasalSync && (saltPayload[5] != 2 && saltPayload[5] != 5) && opcode != (byte)0xBA && opcode != (byte)0xBC && opcode != (byte)0xBE) {
            boolean success = syncBasals(salt, saltPayload[5] < 3);
            if (!success)
                Timber.e("syncing Basal was not successful");
            else
                Timber.d("Successfully synced basals");
            return transmitDataSecurely(opcode, data, waitForOpcodeResponse, timeToWaitForOpcode, waitTimeUnit);
        }

        return transmitDataSecurely(opcode, data, salt, waitForOpcodeResponse, timeToWaitForOpcode, waitTimeUnit);
    }

    public byte[] requestSalt() throws IOException {
        blessConnection();

        BluetoothGattCharacteristic write = gattPipe.getCharacteristic(writeUUID);
        BluetoothGattCharacteristic notify = gattPipe.getCharacteristic(notifyUUID);

        boolean notifySuccess = gattPipe.setCharacteristicNotification(notify, true);
        Timber.v("notifySuccess: " + notifySuccess);

        //request salt
        Timber.d("Request Salt");
        BluetoothGattCharacteristic result = gattPipe.writeCharacteristic(write, new byte[]{0x01}, (byte) 0x02);
        Timber.d("Received salt: %s", Tools.characteristicToString(result));

        byte[] saltPayload = result.getValue();
        //confirm crc

        CRC.verify(saltPayload);
        return saltPayload;
    }

    public synchronized  byte[] transmitDataSecurely(byte opcode, byte[] data, byte[] salt, Byte waitForOpcodeResponse, long timeToWaitForOpcode, TimeUnit waitTimeUnit) throws IOException, PumpNotConnectedException {

        BluetoothGattCharacteristic write = gattPipe.getCharacteristic(writeUUID);
        BluetoothGattCharacteristic notify = gattPipe.getCharacteristic(notifyUUID);

        boolean notifySuccess = gattPipe.setCharacteristicNotification(notify, true);
        Timber.v("notifySuccess: %s", notifySuccess);

        int transmitPayloadSize = salt.length + data.length;
        if (transmitPayloadSize % 16 != 0) {
            transmitPayloadSize = (transmitPayloadSize/16)*16 + 16;
        }

        ByteBuffer bb = ByteBuffer.allocate(transmitPayloadSize);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        bb.put(salt);
        bb.put(data);

        while(bb.hasRemaining()) { //pad remainder with zeros
            bb.put((byte)0);
        }

        Timber.i("PRE-ENCRYPT [%s] %s [CRC CRC]", Tools.bytesToHexString(opcode), Tools.bytesToHexString(bb.array()));

        byte[] key = Settings.getPumpKey().getBytes();//  {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        byte[] encrypted = AuthenticationHashTool.encryptData(bb.array(), key);

        bb = ByteBuffer.allocate(encrypted.length + 3); //+ opcode + CRC
        bb.put(opcode);
        bb.put(encrypted);
        bb.putShort(CRC.calculate(bb.array(), 0, encrypted.length + 1));

        BluetoothGattCharacteristic gattCharacteristic;
        if (waitForOpcodeResponse == null)
            gattCharacteristic = gattPipe.writeCharacteristic(write, bb.array(), timeToWaitForOpcode, waitTimeUnit);
        else
            gattCharacteristic = gattPipe.writeCharacteristic(write, bb.array(), timeToWaitForOpcode, waitTimeUnit, waitForOpcodeResponse);

        byte[] bytes = gattCharacteristic.getValue();
        CRC.verify(bytes);
        byte[] encryptedBytes = Arrays.copyOfRange(bytes, 1, bytes.length-2);
        byte[] decrypted = AuthenticationHashTool.decryptData(encryptedBytes, key);
        Timber.i("RESULT %s", Tools.bytesToHexString(decrypted));

        return decrypted;
    }



    public boolean syncBasals() throws IOException {
        byte[] saltPayload = requestSalt();
        byte[] salt = Arrays.copyOfRange(saltPayload, 1, 5);
        return syncBasals(salt, saltPayload[5] < 3);
    }

    private boolean syncBasals(byte[] salt, boolean isLegacy) throws IOException {
        if (isLegacy) {
            BasalFiveMinuteRxMessage message = new BasalFiveMinuteRxMessage(ActiveBasalModel.get());
            transmitDataSecurely(message.getInitialMessage(), salt, (byte) 0xBB, 4, TimeUnit.SECONDS);
            byte[] content;
            while ((content = message.next()) != null) {
                transmitUnencrypted((byte) 0xBC, content, (byte) 0xBD, 4, TimeUnit.SECONDS);
            }
            byte[] response = transmitDataSecurely(message.getFinalMessage(System.currentTimeMillis(), ScheduledScanReceiver.getTimeOfNextReading()), (byte)0xBF, 2, TimeUnit.SECONDS);
            return response[0] == 1;
        }
        else {
            BasalInterpolationRxMessage message = new BasalInterpolationRxMessage(ActiveBasalModel.get());
            transmitDataSecurely(message.getInitialMessage(), salt, (byte) 0xBB, 4, TimeUnit.SECONDS);
            byte[] content;
            while ((content = message.next()) != null) {
                transmitUnencrypted((byte) 0xBC, content, (byte) 0xBD, 4, TimeUnit.SECONDS);
            }
            byte[] response = transmitDataSecurely(message.getFinalMessage(System.currentTimeMillis(), ScheduledScanReceiver.getTimeOfNextReading()), (byte)0xBF, 2, TimeUnit.SECONDS);
            return response[0] == 1;
        }

    }

    public boolean setMotorPowerLevel(int motorIndex, int powerLevel) throws IOException {
        MotorPowerRxMessage motorPowerRxMessage = new MotorPowerRxMessage(motorIndex, powerLevel);
        byte[] response = transmitDataSecurely((byte)15, motorPowerRxMessage.getPayload(), (byte)16, 2, TimeUnit.SECONDS);
        return response[0] == 1;
    }


    public static void main(String[] args) {
//        byte[] payload = new byte[]{0x03, (byte) 0xf5, 0x45, (byte) 0xe5, (byte) 0xd7, (byte) (byte) 0x89, 0x63, (byte) 0xc6, (byte) 0xf1, (byte) (byte) 0xc5, 0x2f, 0x68, (byte) 0x89, 0x75, (byte) 0xc3, (byte) (byte) 0xc1, 0x0f, (byte) 0xa7, 0x3f};
        byte[] payload = new byte[]{(byte) 0xf5, 0x45, (byte) 0xe5, (byte) 0xd7, (byte) (byte) 0x89, 0x63, (byte) 0xc6, (byte) 0xf1, (byte) (byte) 0xc5, 0x2f, 0x68, (byte) 0x89, 0x75, (byte) 0xc3, (byte) (byte) 0xc1, 0x0f};
        byte[] decryptedData = AuthenticationHashTool.decryptData(payload, "qwertyuiopasdfgh".getBytes());
        System.out.println(Tools.bytesToHexString(decryptedData));
        ByteBuffer bb = ByteBuffer.wrap(decryptedData);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        bb.getFloat();
        bb.getFloat();
        System.out.println(bb.getFloat());
    }

    @Override
    public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
        Timber.v("onCharacteristicChanged: %s", Tools.characteristicToString(characteristic));
        super.onCharacteristicChanged(gatt, characteristic);
    }

    public  void driveMotor(boolean isInsulin, boolean forward, int speed) throws IOException {
        driveMotor(isInsulin, forward ? 1 : -1, (byte)speed);
    }

    public void stopMotor(boolean isInsulin) throws IOException {
        driveMotor(isInsulin, 0, (byte)0);
    }

    private  void driveMotor(boolean isInsulin, int direction, byte speed) throws IOException {
        byte opcode = (byte) 0x97;

        ByteBuffer bb = ByteBuffer.allocate(6);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        bb.put(isInsulin ? (byte)0 : (byte)1);
        bb.put((byte)direction);
        bb.put(speed);
        Timber.d("about to transmit drive");
        transmitDataSecurely(opcode, bb.array(), null, 2, TimeUnit.SECONDS);
    }

    public InjectionResult injectManual(LogEntry entry, byte verificationId) throws IOException {

        float insulinDose = (entry.basalDose + entry.carbDose + entry.insulinDose) - entry.basalInjection - entry.carbInjection - entry.insulinInjection;
        float symlinDose = entry.symlinDose - entry.symlinInjection;

        return injectManual(insulinDose, symlinDose, verificationId, false);
    }

//    public InjectionResult inject(float insulin, float symlin, byte verificationId) throws IOException {
//        return inject(insulin, symlin, verificationId, 0);
//    }

    public InjectionResult injectManual(float insulin, float symlin, byte verificationId, boolean unsafe) throws IOException {

        float insulinDose = Math.max(0, insulin);
        float symlinDose = Math.max(0, symlin);
        if (!unsafe) {
            insulinDose = Math.min(insulinDose, Settings.getMaxInsulinDose());
            symlinDose = Math.min(symlinDose, Settings.getMaxSymlinDose());
        }

        DoseTxMessage doseTxMessage = new DoseTxMessage(insulinDose, symlinDose, 0, verificationId, unsafe);
        Timber.i("Manual Injection: %su insulin, %su symlin", insulinDose, symlinDose);

        byte[] result = transmitDataSecurely((byte)0x33, doseTxMessage.getPayload(), (byte)0x04, 3, TimeUnit.MINUTES);

        return parseInjectionResult(verificationId, result);
    }

    public InjectionResult setTempBasal(boolean isPercent, float tempBasalAmount, long tempBasalDuration) throws IOException {
        byte verificationId = PumpBrain.verificationId.incrementAndGetByte();
        DoseTxMessage doseTxMessage = new DoseTxMessage(0, 0, 0, verificationId, false, 0, true, isPercent, tempBasalAmount, tempBasalDuration);
        byte[] result = transmitDataSecurely((byte)0x33, doseTxMessage.getPayload(), (byte)0x04, 3, TimeUnit.MINUTES);
        return parseInjectionResult(verificationId, result);
    }


    public InjectionResult injectAuto(float insulin, float symlin, byte verificationId, float tempBasalPercent, long tempBasalDuration) throws IOException {
        boolean unsafe = false;
        float insulinDose = Math.max(0, insulin);
        float symlinDose = Math.max(0, symlin);
        insulinDose = Math.min(insulinDose, Settings.getMaxInsulinDose());
        symlinDose = Math.min(symlinDose, Settings.getMaxSymlinDose());

        DoseTxMessage doseTxMessage = new DoseTxMessage(insulinDose, symlinDose, 0, verificationId, unsafe, 0, true, true, tempBasalPercent, tempBasalDuration);
        Timber.i("Auto Injection: %su insulin, %su symlin", insulinDose, symlinDose);



        byte[] result = transmitDataSecurely((byte)0x3, doseTxMessage.getPayload(), (byte)0x04, 3, TimeUnit.MINUTES);

        return parseInjectionResult(verificationId, result);
    }


    @NonNull
    private InjectionResult parseInjectionResult(int verificationId, byte[] result) {
        Timber.v("parseInjectionResult %s", Tools.bytesToHexString(result));



        ByteBuffer bb = ByteBuffer.wrap(result);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        float insulinInjected = DoseEncoder.decode(bb.getChar());
        float symlinInjected = DoseEncoder.decode(bb.getChar());
        float glucagonInjected = DoseEncoder.decode(bb.getChar());

        byte flags = bb.get();
        boolean insulinOcclusion =              (flags & 0b00000001) != 0;
        boolean symlinOcclusion =               (flags & 0b00000010) != 0;
        boolean glucagonOcclusion =             (flags & 0b00000100) != 0;
        boolean unreportedBasalInsulin =        (flags & 0b00001000) != 0;
        boolean unreportedBasalSymlin =         (flags & 0b00010000) != 0;

        boolean glucagonEncoderError =          (flags & 0b00100000) != 0;
        boolean insulinEncoderError =           (flags & 0b01000000) != 0;
        boolean symlinEncoderError =            (flags & 0b10000000) != 0;


        byte returnedVerificationId = bb.get();
        float batteryLevel = DoseEncoder.decode(bb.getChar());
        BatteryTracker.updateVoltage(batteryLevel);


        InjectionResult injectionResult = new InjectionResult(insulinInjected, symlinInjected, glucagonInjected, insulinOcclusion, symlinOcclusion, glucagonOcclusion, insulinEncoderError, symlinEncoderError, glucagonEncoderError, unreportedBasalInsulin, batteryLevel);
        Timber.i("INJECTION RESULT %s", injectionResult);

        if (returnedVerificationId != verificationId) {
            throw new SecurityException("Verification ID did not match. Expected: 0x" + Integer.toHexString(verificationId) + " received: 0x" + Integer.toHexString(returnedVerificationId) + ". Somebody may be trying to kill you.");
        }

        return injectionResult;
    }

    public PumpStateRxMessage synchronizePhoneState() throws IOException {
        Timber.d("Synchronizing phone state...");

        byte[] result = transmitDataSecurely((byte)17, new byte[]{}, (byte)18, 4, TimeUnit.SECONDS);
        return new PumpStateRxMessage(result);
    }

    public PumpStateRxMessage confirmBasals(float insulin, float symlin, float glucagon) throws IOException {
        Timber.d("Confirming basals...");

        BasalConfirmationTxMessage txMessage = new BasalConfirmationTxMessage(insulin, symlin, glucagon);
        byte[] result = transmitDataSecurely((byte)19, txMessage.getBytes(), (byte)18, 4, TimeUnit.SECONDS);
        return new PumpStateRxMessage(result);
    }



    public InjectionResult getLastInjectionResult(int verificationId) throws IOException {
        Timber.d("Getting last injection");
        ByteBuffer bb = ByteBuffer.allocate(4);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        bb.putInt(verificationId);

        byte[] result = transmitDataSecurely((byte)0x5, bb.array(), (byte)0x04, 4, TimeUnit.SECONDS);
        return parseInjectionResult(verificationId, result);
    }

    public void clearOcclusion() throws IOException {
        transmitDataSecurely((byte)0x07, new byte[]{}, (byte) 0x08, 4, TimeUnit.SECONDS);
    }

    public void clearEncoderError() throws IOException {
        transmitDataSecurely((byte)21, new byte[]{}, (byte) 22, 4, TimeUnit.SECONDS);
    }

    public void resetEncoder(boolean isInsulin) throws IOException {
        transmitDataSecurely((byte)0x09, new byte[]{isInsulin ? (byte)0 : (byte)1}, (byte) 10, 4, TimeUnit.SECONDS);
    }

    public ExecutorService getThreadPool() {
        StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
        Timber.v("you got me...%s", stackTrace[stackTrace.length-2]);
        return threadPool;
    }


    public BluetoothDevice getDevice(){
        if (device == null) {
            String address = Settings.getPumpBluetoothAddress();
            if (address != null)
                device = ((BluetoothManager) App.context.getSystemService(Context.BLUETOOTH_SERVICE)).getAdapter().getRemoteDevice(address);
            else
                return null;
        }
        return device;
    }

    public void setDevice(BluetoothDevice device) {
        Settings.setPumpBluetoothAddress(device.getAddress());
        this.device = device;
    }


    public void setMotorPowerLevel(final int motorIndex, final int powerLevel, final SuccessListener result) {

        PumpBluetooth.getInstance().getThreadPool().submit(new Runnable() {
            @Override
            public void run() {
                try {
                    PumpBluetooth.getInstance().setMotorPowerLevel(motorIndex, powerLevel);
                    result.onSuccess();
                } catch (Exception e) {
                    Timber.e(e);
                    result.onFailure();
                }
            }
        });
    }


    public SelfTestResult runSelfTest() throws IOException {
        byte[] result = transmitDataSecurely((byte) 0xDD, new byte[]{}, (byte) 0xDE, 60, TimeUnit.SECONDS);

        ByteBuffer bb = ByteBuffer.wrap(result);
        bb.order(ByteOrder.LITTLE_ENDIAN);

        boolean success = bb.get() != 0;
        float dose = DoseEncoder.decode(bb.getChar());
        float injection = DoseEncoder.decode(bb.getChar());
        boolean occlusion = bb.get() != 0;
        boolean encoderError = bb.get() != 0;
        boolean variation = bb.get() != 0;

        SelfTestResult testResult = new SelfTestResult(success, dose, injection, occlusion, encoderError, variation);
        Timber.d("Test Result: %s", testResult.toString());
        return testResult;

    };


    public int calibrate() throws IOException {
        byte[] result = transmitDataSecurely((byte) 0xDC, new byte[]{}, (byte) 0xDC, 60, TimeUnit.SECONDS);

        //default 464

        ByteBuffer bb = ByteBuffer.wrap(result);
        bb.order(ByteOrder.LITTLE_ENDIAN);

        int offset = bb.getInt();

        Timber.d("Calibration Offset: %s", offset);

        return offset;

    };
}
