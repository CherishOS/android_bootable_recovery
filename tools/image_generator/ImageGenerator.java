/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.recovery.tools;

import java.awt.Color;
import java.awt.Font;
import java.awt.FontFormatException;
import java.awt.FontMetrics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.StringTokenizer;

import javax.imageio.ImageIO;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.GnuParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/**
 * Command line tool to generate the localized image for recovery mode.
 */
public class ImageGenerator {
  // Initial height of the image to draw.
  private static final int INITIAL_HEIGHT = 20000;

  private static final float DEFAULT_FONT_SIZE = 40;

  // This is the canvas we used to draw texts.
  private BufferedImage mBufferedImage;

  // The width in pixels of our image. Once set, its value won't change.
  private final int mImageWidth;

  // The current height in pixels of our image. We will adjust the value when drawing more texts.
  private int mImageHeight;

  // The current vertical offset in pixels to draw the top edge of new text strings.
  private int mVerticalOffset;

  // The font size to draw the texts.
  private final float mFontSize;

  // The name description of the text to localize. It's used to find the translated strings in the
  // resource file.
  private final String mTextName;

  // The directory that contains all the needed font files (e.g. ttf, otf, ttc files).
  private final String mFontDirPath;

  // An explicit map from language to the font name to use.
  // The map is extracted from frameworks/base/data/fonts/fonts.xml.
  // And the language-subtag-registry is found in:
  // https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
  private static final String DEFAULT_FONT_NAME = "Roboto-Regular";
  private static final Map<String, String> LANGUAGE_TO_FONT_MAP = new TreeMap<String, String>() {{
    put("am", "NotoSansEthiopic-Regular");
    put("ar", "NotoNaskhArabicUI-Regular");
    put("as", "NotoSansBengaliUI-Regular");
    put("bn", "NotoSansBengaliUI-Regular");
    put("fa", "NotoNaskhArabicUI-Regular");
    put("gu", "NotoSansGujaratiUI-Regular");
    put("hi", "NotoSansDevanagariUI-Regular");
    put("hy", "NotoSansArmenian-Regular");
    put("iw", "NotoSansHebrew-Regular");
    put("ja", "NotoSansCJK-Regular");
    put("ka", "NotoSansGeorgian-Regular");
    put("ko", "NotoSansCJK-Regular");
    put("km", "NotoSansKhmerUI-Regular");
    put("kn", "NotoSansKannadaUI-Regular");
    put("lo", "NotoSansLaoUI-Regular");
    put("ml", "NotoSansMalayalamUI-Regular");
    put("mr", "NotoSansDevanagariUI-Regular");
    put("my", "NotoSansMyanmarUI-Regular");
    put("ne", "NotoSansDevanagariUI-Regular");
    put("or", "NotoSansOriya-Regular");
    put("pa", "NotoSansGurmukhiUI-Regular");
    put("si", "NotoSansSinhala-Regular");
    put("ta", "NotoSansTamilUI-Regular");
    put("te", "NotoSansTeluguUI-Regular");
    put("th", "NotoSansThaiUI-Regular");
    put("ur", "NotoNaskhArabicUI-Regular");
    put("zh", "NotoSansCJK-Regular");
  }};

  // Languages that write from right to left.
  private static final Set<String> RTL_LANGUAGE = new HashSet<String>() {{
    add("ar"); // Arabic
    add("fa"); // Persian
    add("he"); // Hebrew
    add("iw"); // Hebrew
    add("ur"); // Urdu
  }};

  // Languages that breaks on arbitrary characters.
  // TODO(xunchang) switch to icu library if possible.
  private static final Set<String> LOGOGRAM_LANGUAGE = new HashSet<String>() {{
    add("ja"); // Japanese
    add("km"); // Khmer
    add("ko"); // Korean
    add("lo"); // Lao
    add("zh"); // Chinese
  }};

  /**
   * Exception to indicate the failure to find the translated text strings.
   */
  public static class LocalizedStringNotFoundException extends Exception {
    public LocalizedStringNotFoundException(String message) {
      super(message);
    }

    public LocalizedStringNotFoundException(String message, Throwable cause) {
      super(message, cause);
    }
  }

  /**
   * Initailizes the fields of the image image.
   */
  public ImageGenerator(int imageWidth, String textName, float fontSize, String fontDirPath) {
    mImageWidth = imageWidth;
    mImageHeight = INITIAL_HEIGHT;
    mVerticalOffset = 0;

    // Initialize the canvas with the default height.
    mBufferedImage = new BufferedImage(mImageWidth, mImageHeight, BufferedImage.TYPE_BYTE_GRAY);

    mTextName = textName;
    mFontSize = fontSize;
    mFontDirPath = fontDirPath;
  }

  /**
   * Finds the translated text string for the given textName by parsing the resourceFile.
   * Example of the xml fields:
   * <resources xmlns:android="http://schemas.android.com/apk/res/android">
   *   <string name="recovery_installing_security" msgid="9184031299717114342">
   * "Sicherheitsupdate wird installiert"</string>
   * </resources>
   *
   * @param resourceFile the input resource file in xml format.
   * @param textName the name description of the text.
   *
   * @return the string representation of the translated text.
   */
  private String getTextString(File resourceFile, String textName) throws IOException,
      ParserConfigurationException, org.xml.sax.SAXException, LocalizedStringNotFoundException {
    DocumentBuilderFactory builder = DocumentBuilderFactory.newInstance();
    DocumentBuilder db = builder.newDocumentBuilder();

    Document doc = db.parse(resourceFile);
    doc.getDocumentElement().normalize();

    NodeList nodeList = doc.getElementsByTagName("string");
    for (int i = 0; i < nodeList.getLength(); i++) {
      Node node = nodeList.item(i);
      String name = node.getAttributes().getNamedItem("name").getNodeValue();
      if (name.equals(textName)) {
        return node.getTextContent();
      }
    }

    throw new LocalizedStringNotFoundException(textName + " not found in "
        + resourceFile.getName());
  }

  /**
   * Constructs the locale from the name of the resource file.
   */
  private Locale getLocaleFromFilename(String filename) throws IOException {
    // Gets the locale string by trimming the top "values-".
    String localeString = filename.substring(7);
    if (localeString.matches("[A-Za-z]+")) {
      return Locale.forLanguageTag(localeString);
    }
    if (localeString.matches("[A-Za-z]+-r[A-Za-z]+")) {
      // "${Language}-r${Region}". e.g. en-rGB
      String[] tokens = localeString.split("-r");
      return Locale.forLanguageTag(String.join("-", tokens));
    }
    if (localeString.startsWith("b+")) {
      // The special case of b+sr+Latn, which has the form "b+${Language}+${ScriptName}"
      String[] tokens = localeString.substring(2).split("\\+");
      return Locale.forLanguageTag(String.join("-", tokens));
    }

    throw new IOException("Unrecognized locale string " + localeString);
  }

  /**
   * Iterates over the xml files in the format of values-$LOCALE/strings.xml under the resource
   * directory and collect the translated text.
   *
   * @param resourcePath the path to the resource directory
   *
   * @return a map with the locale as key, and translated text as value
   *
   * @throws LocalizedStringNotFoundException if we cannot find the translated text for the given
   *    locale
   **/
  public Map<Locale, String> readLocalizedStringFromXmls(String resourcePath) throws
      IOException, LocalizedStringNotFoundException {
    File resourceDir = new File(resourcePath);
    if (!resourceDir.isDirectory()) {
      throw new LocalizedStringNotFoundException(resourcePath + " is not a directory.");
    }

    Map<Locale, String> result =
        // Overrides the string comparator so that sr is sorted behind sr-Latn. And thus recovery
        // can find the most relevant locale when going down the list.
        new TreeMap<>((Locale l1, Locale l2) -> {
          if (l1.toLanguageTag().equals(l2.toLanguageTag())) {
            return 0;
          }
          if (l1.getLanguage().equals(l2.toLanguageTag())) {
            return -1;
          }
          if (l2.getLanguage().equals(l1.toLanguageTag())) {
            return 1;
          }
          return l1.toLanguageTag().compareTo(l2.toLanguageTag());
        });

    // Find all the localized resource subdirectories in the format of values-$LOCALE
    String[] nameList = resourceDir.list(
        (File file, String name) -> name.startsWith("values-"));
    for (String name : nameList) {
      File textFile = new File(resourcePath, name + "/strings.xml");
      String localizedText;
      try {
        localizedText = getTextString(textFile, mTextName);
      } catch (IOException | ParserConfigurationException | org.xml.sax.SAXException e) {
        throw new LocalizedStringNotFoundException(
            "Failed to read the translated text for locale " + name, e);
      }

      Locale locale = getLocaleFromFilename(name);
      // Removes the double quotation mark from the text.
      result.put(locale, localizedText.substring(1, localizedText.length() - 1));
    }

    return result;
  }

  /**
   * Returns a font object associated given the given locale
   *
   * @throws IOException if the font file fails to open
   * @throws FontFormatException if the font file doesn't have the expected format
   */
  private Font loadFontsByLocale(String language) throws IOException, FontFormatException {
    String fontName = LANGUAGE_TO_FONT_MAP.getOrDefault(language, DEFAULT_FONT_NAME);
    String[] suffixes = {".otf", ".ttf", ".ttc"};
    for (String suffix : suffixes ) {
      File fontFile = new File(mFontDirPath, fontName + suffix);
      if (fontFile.isFile()) {
        return Font.createFont(Font.TRUETYPE_FONT, fontFile).deriveFont(mFontSize);
      }
    }

    throw new IOException("Can not find the font file " + fontName + " for language " + language);
  }

  /**
   * Separates the text string by spaces and wraps it by words.
  **/
  private List<String> wrapTextByWords(String text, FontMetrics metrics) {
    List<String> wrappedText = new ArrayList<>();
    StringTokenizer st = new StringTokenizer(text, " \n");

    // TODO(xunchang). We assume that all words can fit on the screen. Raise an
    // IllegalStateException if the word is wider than the image width.
    StringBuilder line = new StringBuilder();
    while (st.hasMoreTokens()) {
      String token = st.nextToken();
      if (metrics.stringWidth(line + token + " ") > mImageWidth) {
        wrappedText.add(line.toString());
        line = new StringBuilder();
      }
      line.append(token).append(" ");
    }
    wrappedText.add(line.toString());

    return wrappedText;
  }

  /**
   * One character is a word for CJK.
   */
  private List<String> wrapTextByCharacters(String text, FontMetrics metrics) {
    List<String> wrappedText = new ArrayList<>();

    StringBuilder line = new StringBuilder();
    for (char token : text.toCharArray()) {
      if (metrics.stringWidth(line + Character.toString(token)) > mImageWidth) {
        wrappedText.add(line.toString());
        line = new StringBuilder();
      }
      line.append(token);
    }
    wrappedText.add(line.toString());

    return wrappedText;
  }

  /**
   * Wraps the text with a maximum of mImageWidth pixels per line.
   *
   * @param text the string representation of text to wrap
   * @param metrics the metrics of the Font used to draw the text; it gives the width in pixels of
   *    the text given its string representation
   *
   * @return a list of strings with their width smaller than mImageWidth pixels
   */
  private List<String> wrapText(String text, FontMetrics metrics, String language) {
    if (LOGOGRAM_LANGUAGE.contains(language)) {
      return wrapTextByCharacters(text, metrics);
    }

    return wrapTextByWords(text, metrics);
  }

  /**
   * Encodes the information of the text image for |locale|.
   * According to minui/resources.cpp, the width, height and locale of the image is decoded as:
   *   int w = (row[1] << 8) | row[0];
   *   int h = (row[3] << 8) | row[2];
   *   __unused int len = row[4];
   *   char* loc = reinterpret_cast<char*>(&row[5]);
  */
  private List<Integer> encodeTextInfo(int width, int height, String locale) {
    List<Integer> info = new ArrayList<>(Arrays.asList(width & 0xff, width >> 8,
        height & 0xff, height >> 8, locale.length()));

    byte[] localeBytes = locale.getBytes();
    for (byte b: localeBytes) {
      info.add((int)b);
    }
    info.add(0);

    return info;
  }

  /**
   * Draws the text string on the canvas for given locale.
   *
   * @param text the string to draw on canvas
   * @param locale the current locale tag of the string to draw
   *
   * @throws IOException if we cannot find the corresponding font file for the given locale.
   * @throws FontFormatException if we failed to load the font file for the given locale.
   */
  private void drawText(String text, Locale locale, String languageTag, boolean centralAlignment)
      throws IOException, FontFormatException  {
    Graphics2D graphics = mBufferedImage.createGraphics();
    graphics.setColor(Color.WHITE);
    graphics.setRenderingHint(
        RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_GASP);
    graphics.setFont(loadFontsByLocale(locale.getLanguage()));

    System.out.println("Encoding \"" + locale + "\" as \"" + languageTag + "\": " + text);

    FontMetrics fontMetrics = graphics.getFontMetrics();
    List<String> wrappedText = wrapText(text, fontMetrics, locale.getLanguage());

    // Marks the start y offset for the text image of current locale; and reserves one line to
    // encode the image metadata.
    int currentImageStart = mVerticalOffset;
    mVerticalOffset += 1;
    for (String line : wrappedText) {
      int lineHeight = fontMetrics.getHeight();
      // Doubles the height of the image if we are short of space.
      if (mVerticalOffset + lineHeight >= mImageHeight) {
        resizeHeight(mImageHeight * 2);
      }

      // Draws the text at mVerticalOffset and increments the offset with line space.
      int baseLine = mVerticalOffset + lineHeight - fontMetrics.getDescent();

      // Draws from right if it's an RTL language.
      int x = centralAlignment ? (mImageWidth - fontMetrics.stringWidth(line)) / 2 :
          RTL_LANGUAGE.contains(languageTag) ? mImageWidth - fontMetrics.stringWidth(line) : 0;

      graphics.drawString(line, x, baseLine);

      mVerticalOffset += lineHeight;
    }

    // Encodes the metadata of the current localized image as pixels.
    int currentImageHeight = mVerticalOffset - currentImageStart - 1;
    List<Integer> info = encodeTextInfo(mImageWidth, currentImageHeight, languageTag);
    for (int i = 0; i < info.size(); i++) {
      int pixel[] =  { info.get(i) };
      mBufferedImage.getRaster().setPixel(i, currentImageStart, pixel);
    }
  }

  /**
   * Redraws the image with the new height.
   *
   * @param height the new height of the image in pixels.
   */
  private void resizeHeight(int height) {
    BufferedImage resizedImage =
        new BufferedImage(mImageWidth, height, BufferedImage.TYPE_BYTE_GRAY);
    Graphics2D graphic = resizedImage.createGraphics();
    graphic.drawImage(mBufferedImage, 0, 0, null);
    graphic.dispose();

    mBufferedImage = resizedImage;
    mImageHeight = height;
  }

  /**
   *  This function draws the font characters and saves the result to outputPath.
   *
   * @param localizedTextMap a map from locale to its translated text string
   * @param outputPath the path to write the generated image file.
   *
   * @throws FontFormatException if there's a format error in one of the font file
   * @throws IOException if we cannot find the font file for one of the locale, or we failed to
   *    write the image file.
   */
  public void generateImage(Map<Locale, String> localizedTextMap, String outputPath) throws
      FontFormatException, IOException {
    Map<String, Integer> languageCount = new TreeMap<>();
    for (Locale locale : localizedTextMap.keySet()) {
      String language = locale.getLanguage();
      languageCount.put(language, languageCount.getOrDefault(language, 0) + 1 );
    }

    for (Locale locale : localizedTextMap.keySet()) {
      Integer count = languageCount.get(locale.getLanguage());
      // Recovery expects en-US instead of en_US.
      String languageTag = locale.toLanguageTag();
      if (count == 1) {
        // Make the last country variant for a given language be the catch-all for that language.
        languageTag = locale.getLanguage();
      } else {
        languageCount.put(locale.getLanguage(), count - 1);
      }

      drawText(localizedTextMap.get(locale), locale, languageTag, false);
    }

    // TODO(xunchang) adjust the width to save some space if all texts are smaller than imageWidth.
    resizeHeight(mVerticalOffset);
    ImageIO.write(mBufferedImage, "png", new File(outputPath));
  }

  public static void printUsage(Options options) {
    new HelpFormatter().printHelp("java -jar path_to_jar [required_options]", options);

  }

  public static Options createOptions() {
    Options options = new Options();
    options.addOption(OptionBuilder
        .withLongOpt("image_width")
        .withDescription("The initial width of the image in pixels.")
        .hasArgs(1)
        .isRequired()
        .create());

    options.addOption(OptionBuilder
        .withLongOpt("text_name")
        .withDescription("The description of the text string, e.g. recovery_erasing")
        .hasArgs(1)
        .isRequired()
        .create());

    options.addOption(OptionBuilder
        .withLongOpt("font_dir")
        .withDescription("The directory that contains all the support font format files, e.g."
            + " $OUT/system/fonts/")
        .hasArgs(1)
        .isRequired()
        .create());

    options.addOption(OptionBuilder
        .withLongOpt("resource_dir")
        .withDescription("The resource directory that contains all the translated strings in xml"
            + " format, e.g. bootable/recovery/tools/recovery_l10n/res/")
        .hasArgs(1)
        .isRequired()
        .create());

    options.addOption(OptionBuilder
        .withLongOpt("output_file")
        .withDescription("Path to the generated image")
        .hasArgs(1)
        .isRequired()
        .create());

    return options;
  }

  public static void main(String[] args) throws NumberFormatException, IOException,
      FontFormatException, LocalizedStringNotFoundException {
    Options options = createOptions();
    CommandLine cmd;
    try {
      cmd = new GnuParser().parse(options, args);
    } catch (ParseException e) {
      System.err.println(e.getMessage());
      printUsage(options);
      return;
    }

    int imageWidth = Integer.parseUnsignedInt(cmd.getOptionValue("image_width"));

    ImageGenerator imageGenerator = new ImageGenerator(imageWidth, cmd.getOptionValue("text_name"),
        DEFAULT_FONT_SIZE, cmd.getOptionValue("font_dir"));

    Map<Locale, String> localizedStringMap =
        imageGenerator.readLocalizedStringFromXmls(cmd.getOptionValue("resource_dir"));
    imageGenerator.generateImage(localizedStringMap, cmd.getOptionValue("output_file"));
  }
}

