/* Copyright (c) 2008, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

package java.io;

public class RandomAccessFile {
  private long peer;
  private long length;
  private long position = 0;
  
  public RandomAccessFile(String name, String mode)
    throws FileNotFoundException
  {
    if (! mode.equals("r")) throw new IllegalArgumentException();

    long[] result = new long[2];
    open(name, result);
    peer = result[0];
    length = result[1];
  }

  private static native void open(String name, long[] result)
    throws FileNotFoundException;

  public long length() throws IOException {
    return length;
  }

  public long getFilePointer() throws IOException {
    return position;
  }

  public void seek(long position) throws IOException {
    if (position < 0 || position > length) throw new IOException();

    this.position = position;
  }

  public void readFully(byte[] buffer, int offset, int length)
    throws IOException
  {
    if (peer == 0) throw new IOException();

    if (length == 0) return;

    if (position + length > this.length) throw new EOFException();

    if (offset < 0 || offset + length > buffer.length)
      throw new ArrayIndexOutOfBoundsException();

    copy(peer, position, buffer, offset, length);
  }

  private static native void copy(long peer, long position, byte[] buffer,
                                  int offset, int length);

  public void close() throws IOException {
    if (peer != 0) {
      close(peer);
      peer = 0;
    }
  }

  private static native void close(long peer);
}
