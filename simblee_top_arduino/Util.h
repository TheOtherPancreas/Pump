#ifndef Util_h
#define Util_h



short bytesToShort(char one, char two) {
  short result = one & (short)0x00FF;
  result += (((short)two) << 8) & (short)0xFF00;
  return result;
}

void intToBytes(int value, char* buff, int offset) {
  
}

int bytesToInt(char* buff, int offset) {
  
  
  int a = ((((int)buff[offset+3]) << 24) & 0xFF000000);
  int b = ((((int)buff[offset+2]) << 16) & 0x00FF0000);
  int c = ((((int)buff[offset+1]) << 8) & 0x0000FF00);
  int d = ((((int)buff[offset+0])) & 0x000000FF);
  int result = a + b + c + d;

  return result;
}


long bytesToLong(char* buff, int offset) {  
  int a = ((((int)buff[offset+7]) << 56) & 0xFF00000000000000);
  int b = ((((int)buff[offset+6]) << 48) & 0x00FF000000000000);
  int c = ((((int)buff[offset+5]) << 40) & 0x0000FF0000000000);
  int d = ((((int)buff[offset+4]) << 32) & 0x000000FF00000000);
  int e = ((((int)buff[offset+3]) << 24) & 0x00000000FF000000);
  int f = ((((int)buff[offset+2]) << 16) & 0x0000000000FF0000);
  int g = ((((int)buff[offset+1]) << 8)  & 0x000000000000FF00);
  int h = ((((int)buff[offset+0]))       & 0x00000000000000FF);
  int result = a + b + c + d + e + f + g + h;
 
  return result;
}

char byteOneOfShort(short val) {
  return (char)(val >> 8);
}

char byteTwoOfShort(short val) {
  return (char)val;
}

void printHex(char *data, int length) {
  for (int i = 0; i < length; i++) {
    if (i != 0)
      Serial.print(",");
    Serial.print((int)data[i], HEX);    
  }
  Serial.println();
}

short crc(char* data, int offset, int length) {

  short crc = 0;

  for (int i = 0; i < length; i++) {    
    crc ^= ((short)data[i + offset]) << 8;
    for(int j = 0; j < 8; j++){
      if ((crc & 0x8000) != 0) {
        crc = (short)((short)(crc << 1) ^ 0x1021);
      } else {
        crc = (short)(crc << 1);
      }
    }
  }

  return crc;
}


bool verifyCRC(char* data, int len) {
  short expected = bytesToShort(data[len-1], data[len-2]);
  short calculated = crc(data, 0, len-2);
  Serial.print("[verify]expected: ");
  Serial.print(expected);
  Serial.print(" == calculated: ");
  Serial.println(calculated);
  return calculated == expected;
}



const char* createAdvertisementData(char* str) {
  
  short hash = crc(str, 0, 16);
  
  char* result = new char[5];
  String res = String(byteOneOfShort(hash), HEX) + String(byteTwoOfShort(hash), HEX);
  res.toCharArray(result, 5);
  Serial.print("Advertisement Data: ");
  Serial.println(result);
  return result;
}

/**
 * Due to the limited number of bytes we can transmit over bluetooth, as well as the limited number
 * of bytes we can store in flash on the Simblee, we encode doses and injections in 2 bytes.
 * This encoding allows for 0.005u accuracy, and up to 327.67 units
 */
float decodeDose(short val) {
  return val / 200.0f;
}

void encodeDose(float dose, char* buf, int offset) {
  int val;
  if (dose * 200 > 0xFFFF) {
    val = 0xFFFF;
  }
  else if (dose < 0) {
    val = 0;
  }
  else {
    val = (int)(dose * 200 + 0.5); //0.5 for rounding
    
  }
  
  buf[offset] = (char)val;
  buf[offset+1] = (char)(val >> 8);
  
}

/**
 * Due to the limited number of bytes we can transmit over bluetooth, as well as the limited number
 * of bytes we can store in flash on the Simblee, we encode doses and injections in 2 bytes.
 * This encoding allows for 0.005u accuracy, and up to 327.67 units
 */
float decodeBasal(short val) {
  return val / 2000.0f;
}

void encodeBasal(float dose, char* buf, int offset) {
  int val;
  if (dose * 2000 > 0xFFFF) {
    val = 0xFFFF;
  }
  else if (dose < 0) {
    val = 0;
  }
  else {
    val = (int)(dose * 2000);
  }
  
  buf[offset] = (char)val;
  buf[offset+1] = (char)(val >> 8);
  
}

void encodeBatteryVoltage(float voltage, char* buf, int offset) {
  encodeDose(voltage, buf, offset);
}

float decodePowerLimit(char limit) {
    return ((byte)limit) / 100.0f;
}

/**
 * Returns a hash code based on the contents of the given array. For any two
 * {@code float} arrays {@code a} and {@code b}, if
 * {@code Arrays.equals(a, b)} returns {@code true}, it means
 * that the return value of {@code Arrays.hashCode(a)} equals {@code Arrays.hashCode(b)}.
 * <p>
 * The value returned by this method is the same value as the
 * {@link List#hashCode()} method which is invoked on a {@link List}
 * containing a sequence of {@link Float} instances representing the
 * elements of array in the same order. If the array is {@code null}, the return
 * value is 0.
 *
 * @param array
 *            the array whose hash code to compute.
 * @return the hash code for {@code array}.
 */
int calculateFloatArrayHashCode(float* array, int len) {
    int hashCode = 1;
    for (int i = 0; i < len; i++) {
      float element = array[i];
      int floatToIntBits = *(int*)&element;
      
        /*
         * the hash code value for float value is
         * Float.floatToIntBits(value)
         */
        hashCode = 31 * hashCode + floatToIntBits;
    }
    return hashCode;
}

/**
 * Returns a hash code based on the contents of the specified array.
 * For any two <tt>char</tt> arrays <tt>a</tt> and <tt>b</tt>
 * such that <tt>Arrays.equals(a, b)</tt>, it is also the case that
 * <tt>Arrays.hashCode(a) == Arrays.hashCode(b)</tt>.
 *
 * <p>The value returned by this method is the same value that would be
 * obtained by invoking the {@link List#hashCode() <tt>hashCode</tt>}
 * method on a {@link List} containing a sequence of {@link Character}
 * instances representing the elements of <tt>a</tt> in the same order.
 * If <tt>a</tt> is <tt>null</tt>, this method returns 0.
 *
 * @param a the array whose hash value to compute
 * @return a content-based hash code for <tt>a</tt>
 * @since 1.5
 */
int calculateShortArrayHashCode(short * a, int len) {
    int result = 1;
    for (int i = 0; i < len; i++) {
      result = 31 * result + a[i];
    }
    return result;
}

#endif
