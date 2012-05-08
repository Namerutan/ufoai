/*
 * generated by Xtext
 */
package net.sourceforge.ufoai.ui.labeling;

import org.eclipse.xtext.resource.IEObjectDescription;
import org.eclipse.xtext.ui.label.DefaultDescriptionLabelProvider;

/**
 * Provides labels for a IEObjectDescriptions and IResourceDescriptions. see
 * http://www.eclipse.org/Xtext/documentation/latest/xtext.html#labelProvider
 */
public class UFOScriptDescriptionLabelProvider extends
		DefaultDescriptionLabelProvider {
	@Override
	public String text(IEObjectDescription ele) {
		return ele.getName().toString();
	}

	@Override
	public String image(IEObjectDescription ele) {
		return text(ele) + ".png";
	}
}
