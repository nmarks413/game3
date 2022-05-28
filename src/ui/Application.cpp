#include <iostream>
#include "ui/Application.h"

namespace Game3 {
	Application::Application(): nanogui::Screen(Eigen::Vector2i(1024, 768), "Game3") {
		setLayout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill));

		auto *button_box = new nanogui::Widget(this);
		button_box->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Minimum, -1, -1));

		auto *b = new nanogui::Button(button_box, "", ENTYPO_ICON_SAVE);
		auto *theme = new nanogui::Theme(*b->theme());
		theme->mButtonCornerRadius = 0;
		b->setTheme(theme);
		b->setCallback([] { std::cout << ":)\n"; });
		b->setTooltip("Save");
		b->setEnabled(false);

		auto *b2 = new nanogui::Button(button_box, "", ENTYPO_ICON_FOLDER);
		b2->setTheme(theme);
		b2->setTooltip("Open");

		performLayout();
	}

	bool Application::keyboardEvent(int key, int scancode, int action, int modifiers) {
		if (Screen::keyboardEvent(key, scancode, action, modifiers))
            return true;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            setVisible(false);
            return true;
        }
        return false;
	}

	void Application::draw(NVGcontext *ctx) {
		Screen::draw(ctx);
	}
}
