/* Copyright (c) 2010-2011, Avian Contributors

   Permission to use, copy, modify, and/or distribute this software
   for any purpose with or without fee is hereby granted, provided
   that the above copyright notice and this permission notice appear
   in all copies.

   There is NO WARRANTY for this software.  See license.txt for
   details. */

package avian;

import java.net.URL;
import java.net.MalformedURLException;
import java.security.CodeSource;
import java.security.AllPermission;
import java.security.Permissions;
import java.security.ProtectionDomain;
import java.security.cert.Certificate;
import java.io.ByteArrayOutputStream;

public class OpenJDK {  
  public static ProtectionDomain getProtectionDomain(VMClass c) {
    CodeSource source = null;
    if (c.source != null) {
      try {
        source = new CodeSource
          (new URL(new String(c.source, 0, c.source.length - 1)),
           (Certificate[]) null);
      } catch (MalformedURLException ignored) { }
    }

    Permissions p = new Permissions();
    p.add(new AllPermission());

    return new ProtectionDomain(source, p);
  }

  private static byte[] replace(int a, int b, byte[] s, int offset,
                                int length)
  {
    byte[] array = new byte[length];
    for (int i = 0; i < length; ++i) {
      byte c = s[i];
      array[i] = (byte) (c == a ? b : c);
    }
    return array;
  }

  public static Class getDeclaringClass(VMClass c) {
    try {
      String name = new String
        (replace('/', '.', c.name, 0, c.name.length - 1), 0,
         c.name.length - 1);
      int index = name.lastIndexOf("$");
      if (index == -1) {
        return null;
      } else {
        return c.loader.loadClass(name.substring(0, index));
      }
    } catch (ClassNotFoundException e) {
      return null;
    }
  }

  private static final char[] Primitives = "VZBCSIFJD".toCharArray();
  private static final int PrimitiveFlag = 1 << 5;

  private static void write(ByteArrayOutputStream out, Class t) {
    VMClass c = SystemClassLoader.vmClass(t);
    if ((c.vmFlags & PrimitiveFlag) != 0) {
      for (char p: Primitives) {
        if (c == Classes.primitiveClass(p)) {
          out.write(p);
        }
      }
    } else if (c.name[0] == '[') {
      out.write(c.name, 0, c.name.length - 1);
    } else {
      out.write('L');
      out.write(c.name, 0, c.name.length - 1);
      out.write(';');
    }
  }

  public static byte[] typeToSpec(Class returnType, Class[] parameterTypes) {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    out.write('(');
    for (Class t: parameterTypes) {
      write(out, t);
    }
    out.write(')');
    write(out, returnType);
    out.write(0);
    return out.toByteArray();
  }
}
